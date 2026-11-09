// CAMERA SHARING (w-camshare): the client's side of the camera configs (the format and the registry: CameraShareCodec.cs).
//   chase     the "chase" section: one of MY CAMERAS (ReflexCameraPresets) as keyed values; a pasted one becomes a new preset (ON in the cycle)
//   fpv       the "fpv" section: the Dynamic FPV settings (registered by ReflexFpvShare.cs, or by the FPV code itself: see there)
//   library   FIRST-PERSON SETUPS: saved FPV configs as .json files in <ReflexSave.Folder>/Cameras (the client's save area, never the game's files);
//             EXPORT writes Cameras/Export/<name>.json, IMPORT .JSON FILES reads every file in Cameras/Import (a camera becomes a preset, an FPV
//             setup a library entry). MY CAMERAS stays the chase cameras' library (save.json's Camera.Presets)
//   clipboard GUIUtility.systemCopyBuffer; tests swap ClipboardGet / ClipboardSet (a test never touches the user's clipboard)
// Nothing here applies a camera by itself: USE THIS CAMERA / USE THIS SETUP do, at once (in a race too: the rig reads the preset every frame, the
// FPV camera reloads its settings).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEngine;

namespace Reflex.Port.Options
{
    public static class ReflexCameraShare
    {
        public const string AuthorKey = "Camera.ShareAuthor";
        public const int MaxSetups = 200;

        // ------------------------------------------------------------------------------------------------ the sections
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot() => EnsureRegistered();

        /// The built-in sections (each only where its owner has not registered one).
        public static void EnsureRegistered()
        {
            CameraShare.RegisterDefault(ChaseSection());
            ReflexFpvShare.RegisterDefault();
        }

        static readonly string[] FovModes = { "SCALE", "FIXED" };

        /// The chase camera's keys (a CameraPreset's fields: ReflexCameraPresets' ranges).
        static CameraShareSection ChaseSection() => new CameraShareSection(CameraShare.ChaseId, 1, "CAMERA", new[]
        {
            CameraShareParam.Choice("base", "BASE VIEW", 1, ReflexCameraPresets.BaseNames),
            CameraShareParam.Choice("fovMode", "FIELD OF VIEW", 0, FovModes),
            CameraShareParam.Whole("fovScale", "FOV SCALE", 100, ReflexCameraPresets.FovScaleMin, ReflexCameraPresets.FovScaleMax, "%"),
            CameraShareParam.Whole("fovDegrees", "FOV DEGREES", 85, ReflexCameraPresets.FovDegreesMin, ReflexCameraPresets.FovDegreesMax, "°"),
            CameraShareParam.Number("distance", "DISTANCE", 0, ReflexCameraPresets.DistanceMin, ReflexCameraPresets.DistanceMax, " FT"),
            CameraShareParam.Number("height", "HEIGHT", 0, ReflexCameraPresets.HeightMin, ReflexCameraPresets.HeightMax, " FT"),
            CameraShareParam.Number("side", "SIDE", 0, -ReflexCameraPresets.SideMax, ReflexCameraPresets.SideMax, " FT"),
            CameraShareParam.Number("pitch", "PITCH", 0, -ReflexCameraPresets.PitchMax, ReflexCameraPresets.PitchMax, "°"),
            CameraShareParam.Number("posLag", "POSITION LAG", 0, 0, ReflexCameraPresets.LagMax, " S"),
            CameraShareParam.Number("rotLag", "ROTATION LAG", 0, 0, ReflexCameraPresets.LagMax, " S"),
            CameraShareParam.Number("roll", "ROLL WITH BIKE", 0, 0, 1),
            CameraShareParam.Number("shake", "SHAKE", 1, 0, 2),
            CameraShareParam.Number("shakeSpeed", "SHAKE: SPEED", 0, 0, 1),
            CameraShareParam.Number("shakeLanding", "SHAKE: LANDINGS", 0, 0, 1),
            CameraShareParam.Number("shakeRough", "SHAKE: ROUGH GROUND", 0, 0, 1),
        });

        public static Dictionary<string, double> ChaseValues(CameraPreset p) => new Dictionary<string, double>(StringComparer.Ordinal)
        {
            ["base"] = p.baseView, ["fovMode"] = p.fovMode, ["fovScale"] = p.fovScale, ["fovDegrees"] = p.fovDegrees,
            ["distance"] = p.distance, ["height"] = p.height, ["side"] = p.side, ["pitch"] = p.pitch,
            ["posLag"] = p.posLag, ["rotLag"] = p.rotLag, ["roll"] = p.rollFollow,
            ["shake"] = p.shake, ["shakeSpeed"] = p.shakeSpeed, ["shakeLanding"] = p.shakeLanding, ["shakeRough"] = p.shakeRough,
        };

        /// A preset (not in the list yet) from a config's chase section.
        public static CameraPreset PresetOf(CameraConfig c)
        {
            var s = c?.Section(CameraShare.ChaseId);
            if (s == null) return null;
            float F(string k, float d) => (float)s.Get(k, d);
            int I(string k, int d) => (int)s.Get(k, d);
            return new CameraPreset
            {
                name = c.name, author = c.author, baseView = I("base", 1), fovMode = I("fovMode", 0), fovScale = I("fovScale", 100), fovDegrees = I("fovDegrees", 85),
                distance = F("distance", 0), height = F("height", 0), side = F("side", 0), pitch = F("pitch", 0), posLag = F("posLag", 0), rotLag = F("rotLag", 0),
                rollFollow = F("roll", 0), shake = F("shake", 1), shakeSpeed = F("shakeSpeed", 0), shakeLanding = F("shakeLanding", 0), shakeRough = F("shakeRough", 0),
            };
        }

        /// The config of one of MY CAMERAS (its name and author; the chase section).
        public static CameraConfig ConfigOf(CameraPreset p)
        {
            EnsureRegistered();
            var c = new CameraConfig { name = p.name, author = string.IsNullOrEmpty(p.author) ? DefaultAuthor : p.author, game = Application.version };
            c.Put(CameraShare.ChaseId, ChaseValues(p));
            return c;
        }

        /// The Dynamic FPV settings in use now as a config.
        public static CameraConfig ConfigOfFpv(string name, string author)
        {
            EnsureRegistered();
            var sec = CameraShare.Section(CameraShare.FpvId);
            var c = new CameraConfig { name = string.IsNullOrWhiteSpace(name) ? "MY FPV" : name, author = author ?? DefaultAuthor, game = Application.version };
            c.Put(CameraShare.FpvId, sec?.Read?.Invoke() ?? sec?.Defaults() ?? new Dictionary<string, double>());
            return c;
        }

        /// The FPV values in use now (null: no FPV section, or it cannot be read).
        public static Dictionary<string, double> FpvNow()
        {
            EnsureRegistered();
            var sec = CameraShare.Section(CameraShare.FpvId);
            try { return sec?.Read == null ? null : sec.Complete(sec.Read(), out _, out _); } catch (Exception e) { Debug.LogWarning("[CameraShare] fpv: " + e.Message); return null; }
        }

        /// The setup's FPV values are the ones in use.
        public static bool FpvInUse(CameraConfig c)
        {
            var s = c?.Section(CameraShare.FpvId); var now = FpvNow();
            return s != null && now != null && CameraConfig.SameValues(s.Values, now);
        }

        /// USE THIS SETUP: the config's FPV values become the settings (every registered key: the missing ones at their defaults); a running FPV camera
        /// takes them at once.
        public static bool ApplyFpv(CameraConfig c)
        {
            var s = c?.Section(CameraShare.FpvId); var sec = CameraShare.Section(CameraShare.FpvId);
            if (s == null || sec?.Apply == null) return false;
            sec.Apply(sec.Complete(s.Values, out _, out _));
            return true;
        }

        /// What a config holds, in one line ("CAMERA · DYNAMIC FPV").
        public static string Contents(CameraConfig c) => c == null ? "" : string.Join("  ·  ", c.Sections.Select(s => CameraShare.Section(s.Id)?.Label ?? s.Id.ToUpperInvariant()));

        /// A section's values, a few per line (the setup page's summary).
        public static string Summary(CameraSectionData s, int max = 40)
        {
            var sec = s != null ? CameraShare.Section(s.Id) : null;
            if (sec == null) return "";
            var parts = sec.Params.Take(max).Select(p => p.Label + " " + p.Format(s.Get(p.Key, p.Default))).ToList();
            var sb = new StringBuilder();
            for (int i = 0; i < parts.Count; i++) { if (i > 0) sb.Append(i % 3 == 0 ? "\n" : "   ·   "); sb.Append(parts[i]); }
            return sb.ToString();
        }

        /// The note a pasted / imported config gets when it came from another version.
        public static string VersionNote(CameraConfig c)
        {
            if (c == null || !c.OtherVersion) return "";
            int unknown = c.UnknownSections + c.Sections.Sum(s => s.Unknown), missing = c.Sections.Sum(s => s.Missing);
            return "OTHER VERSION" + (c.Sections.Any(s => s.Newer) ? " (NEWER)" : "") + (unknown > 0 ? ": " + unknown + " UNKNOWN LEFT OUT" : "") + (missing > 0 ? (unknown > 0 ? ", " : ": ") + missing + " AT THEIR DEFAULT" : "");
        }

        // ------------------------------------------------------------------------------------------------ the author
        /// The AUTHOR a new share starts with (the last one typed).
        public static string DefaultAuthor
        {
            get => ReflexSave.GetString(AuthorKey, "");
            set { ReflexSave.SetString(AuthorKey, CameraConfig.CleanText(value, CameraConfig.MaxAuthor)); ReflexSave.Save(); }
        }

        // ------------------------------------------------------------------------------------------------ the clipboard
        public static Func<string> ClipboardGet = () => GUIUtility.systemCopyBuffer;
        public static Action<string> ClipboardSet = text => GUIUtility.systemCopyBuffer = text;

        public static string CopyShareCode(CameraConfig c) { string code = c.ShareCode(); ClipboardSet(code); return code; }

        /// The config in the clipboard (a share code anywhere in it, or a config's JSON); null + the message (upper case) when there is none.
        public static CameraConfig FromClipboard(out string error)
        {
            EnsureRegistered();
            string text;
            try { text = ClipboardGet() ?? ""; } catch (Exception e) { error = "THE CLIPBOARD CANNOT BE READ: " + e.Message.ToUpperInvariant(); return null; }
            var got = CameraConfig.FromShareText(text, out error);
            if (got == null) { error = (error ?? "NOTHING TO IMPORT").ToUpperInvariant(); return null; }
            got.id = "";
            return got;
        }

        // ------------------------------------------------------------------------------------------------ importing
        public sealed class ImportResult
        {
            public CameraPreset preset;     // the new one of MY CAMERAS
            public CameraConfig setup;      // the new FIRST-PERSON SETUP
            public readonly List<string> problems = new List<string>();
            public bool Any => preset != null || setup != null;
            public string Message(string what)
            {
                var got = new List<string>();
                if (preset != null) got.Add("CAMERA " + preset.name);
                if (setup != null) got.Add("FPV SETUP " + setup.name.ToUpperInvariant());
                string m = got.Count > 0 ? what + " " + string.Join(" AND ", got) : "NOTHING IMPORTED";
                return m + (problems.Count > 0 ? ": " + string.Join("; ", problems) : "");
            }
        }

        /// A pasted or read config into the libraries: its chase camera a new preset of MY CAMERAS (ON in the cycle), its FPV values a new FIRST-PERSON
        /// SETUP. Nothing is applied.
        public static ImportResult Import(CameraConfig c)
        {
            var r = new ImportResult();
            if (c == null) return r;
            if (c.Has(CameraShare.ChaseId))
            {
                var p = ReflexCameraPresets.Import(PresetOf(c));
                if (p != null) r.preset = p; else r.problems.Add("MY CAMERAS IS FULL (" + ReflexCameraPresets.MaxPresets + "): DELETE ONE FIRST");
            }
            if (c.Has(CameraShare.FpvId))
            {
                try
                {
                    var s = c.Clone(); s.id = ""; s.name = SetupName(c.name);
                    SaveSetup(s);
                    r.setup = s;
                }
                catch (Exception e) { r.problems.Add("FPV SETUP NOT SAVED: " + e.Message.ToUpperInvariant()); }
            }
            string note = VersionNote(c);
            if (note.Length > 0) r.problems.Add(note);
            return r;
        }

        // ------------------------------------------------------------------------------------------------ FIRST-PERSON SETUPS (files)
        public static string Folder => Path.Combine(ReflexSave.Folder, "Cameras");
        public static string ExportFolder => Path.Combine(Folder, "Export");
        public static string ImportFolder => Path.Combine(Folder, "Import");

        /// A setup's name: the camera names' rules (letters, digits, spaces, - and _, up to 20, upper case).
        public static string SetupName(string s) { s = ReflexCameraPresets.CleanName(s); return s.Length == 0 ? "FPV SETUP" : s; }

        /// The saved setups (configs with an FPV section), by name.
        public static List<CameraConfig> Setups()
        {
            EnsureRegistered();
            var list = new List<CameraConfig>();
            try
            {
                if (!Directory.Exists(Folder)) return list;
                foreach (var path in Directory.GetFiles(Folder, "*.json").OrderBy(p => p, StringComparer.OrdinalIgnoreCase).Take(MaxSetups))
                {
                    try
                    {
                        if (new FileInfo(path).Length > CameraConfig.MaxJson) continue;
                        var c = CameraConfig.FromJson(File.ReadAllText(path));
                        if (!c.Has(CameraShare.FpvId)) continue;
                        c.id = Path.GetFileNameWithoutExtension(path);
                        list.Add(c);
                    }
                    catch (Exception e) { Debug.LogWarning("[CameraShare] " + Path.GetFileName(path) + ": " + e.Message); }
                }
            }
            catch (Exception e) { Debug.LogWarning("[CameraShare] library: " + e.Message); }
            return list.OrderBy(c => c.name, StringComparer.OrdinalIgnoreCase).ThenBy(c => c.id, StringComparer.Ordinal).ToList();
        }

        public static CameraConfig FindSetup(string id) => string.IsNullOrEmpty(id) ? null : Setups().FirstOrDefault(c => c.id == id);

        static string Slug(string name)
        {
            var sb = new StringBuilder();
            foreach (char ch in (name ?? "").Trim().ToLowerInvariant()) sb.Append(ch < 128 && char.IsLetterOrDigit(ch) ? ch : '-');
            string s = sb.ToString().Trim('-');
            while (s.Contains("--")) s = s.Replace("--", "-");
            return s.Length == 0 ? "camera" : s.Length > 40 ? s.Substring(0, 40).Trim('-') : s;
        }
        static void WriteAtomic(string path, string text)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string tmp = path + ".tmp";
            File.WriteAllText(tmp, text, new UTF8Encoding(false));
            if (File.Exists(path)) File.Delete(path);
            File.Move(tmp, path);
        }

        /// Saves a setup in the library (a new one gets a file of its own); returns its path.
        public static string SaveSetup(CameraConfig c)
        {
            if (ReflexSave.Sandboxed) throw new InvalidOperationException("the save folder is not writable in this run");
            if (string.IsNullOrEmpty(c.id))
            {
                string stem = Slug(c.name), id = stem; int n = 2;
                while (File.Exists(Path.Combine(Folder, id + ".json"))) id = stem + "-" + n++;
                c.id = id;
            }
            c.game = Application.version;
            string path = Path.Combine(Folder, c.id + ".json");
            WriteAtomic(path, c.ToJson());
            return path;
        }

        /// SAVE CURRENT AS NEW SETUP: the FPV settings in use as a new setup ("FPV SETUP", "FPV SETUP 2" ...).
        public static CameraConfig SaveCurrentFpv()
        {
            var names = new HashSet<string>(Setups().Select(s => s.name), StringComparer.OrdinalIgnoreCase);
            string name = "FPV SETUP";
            for (int i = 2; names.Contains(name); i++) name = "FPV SETUP " + i;
            var c = ConfigOfFpv(name, DefaultAuthor);
            SaveSetup(c);
            return c;
        }

        /// SAVE CURRENT SETTINGS INTO IT: the setup's FPV values replaced by the ones in use.
        public static void OverwriteWithCurrent(CameraConfig c)
        {
            var now = FpvNow();
            if (c == null || now == null) return;
            c.Put(CameraShare.FpvId, now);
            SaveSetup(c);
        }

        public static void DeleteSetup(CameraConfig c)
        {
            if (c == null || string.IsNullOrEmpty(c.id)) return;
            string path = Path.Combine(Folder, c.id + ".json");
            if (File.Exists(path)) File.Delete(path);
        }

        /// A copy of a config as a .json file in Cameras/Export; returns the path.
        public static string Export(CameraConfig c)
        {
            string path = Path.Combine(ExportFolder, Slug(c.name) + ".json");
            WriteAtomic(path, c.ToJson());
            return path;
        }

        /// Every .json in Cameras/Import into the libraries (Import; the files stay where they are). The report line.
        public static string ImportFiles()
        {
            EnsureRegistered();
            Directory.CreateDirectory(ImportFolder);
            int cams = 0, setups = 0; var refused = new List<string>();
            foreach (var path in Directory.GetFiles(ImportFolder, "*.json").OrderBy(p => p, StringComparer.OrdinalIgnoreCase).Take(MaxSetups))
            {
                try
                {
                    if (new FileInfo(path).Length > CameraConfig.MaxJson) throw new FormatException("too large");
                    var r = Import(CameraConfig.FromJson(File.ReadAllText(path)));
                    if (r.preset != null) cams++;
                    if (r.setup != null) setups++;
                    if (!r.Any) refused.Add(Path.GetFileName(path) + ": " + string.Join("; ", r.problems));
                }
                catch (Exception e) { refused.Add(Path.GetFileName(path) + ": " + e.Message); }
            }
            if (cams + setups + refused.Count == 0) return "PUT .JSON FILES IN CAMERAS/IMPORT FIRST";
            return "IMPORTED " + cams + " CAMERA(S), " + setups + " FPV SETUP(S)" + (refused.Count > 0 ? ", REFUSED " + refused.Count + ": " + string.Join("; ", refused.Take(2)).ToUpperInvariant() : "");
        }

        /// Opens a folder in the system's file browser (made first).
        public static void Reveal(string dir)
        {
            try { Directory.CreateDirectory(dir); if (!Application.isBatchMode) Application.OpenURL(new Uri(dir + Path.DirectorySeparatorChar).AbsoluteUri); }
            catch (Exception e) { Debug.LogWarning("[CameraShare] " + e.Message); }
        }
    }
}

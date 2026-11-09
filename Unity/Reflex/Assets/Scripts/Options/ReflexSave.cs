// ReflexSave: the client's save data, one readable JSON file (modelled on Open Unleashed's MxaSave / MxaPrefs and its
// _documentation/save-system.md), with the old PlayerPrefs keys kept readable for the original front end and the tests.
//
//   <persistentDataPath>/Save/save.json         every setting: { "format", "version", "game", "saved", "ints": {}, "floats": {},
//                                               "strings": {}, "deleted": [], "migrated": [], "lastAutoBackup" }, keys sorted
//   <persistentDataPath>/Save/save.json.bak     the file before the last write (a write goes to save.json.tmp, then replaces it;
//                                               an unreadable save.json loads the .bak)
//   <persistentDataPath>/Save/Backups/*.zip     "<date> <label>.zip": save.json + the game's own save (ReflexCache/save/**: the
//                                               profile's savegame.bxml) + the per-user graphics.cfg. "auto" once a day (the newest
//                                               10 kept), "manual" (OPTIONS > SYSTEM > BACK UP NOW), "before restore", "before the
//                                               save system" (+ playerprefs.json: the old values as they were)
//   <persistentDataPath>/Save/restore-pending.zip  a chosen backup, applied at the next start before anything reads the save
//
// The old keys (every key starting with "Reflex": ReflexEvent, ReflexAI, ReflexCameraMode, ReflexOriginalHud, ReflexUI.*,
// ReflexAudio.*, ReflexFpv.*, ...) are written by code this client does not own (the original front end's OPTIONS page, the
// event list, the tests), which reads and writes PlayerPrefs. They stay in PlayerPrefs as the live store and the save mirrors
// them: a read through this API reads PlayerPrefs (and refreshes the save's copy), a write goes to both, a write of the file
// first takes PlayerPrefs' current values of every old key it knows. At start, an old key PlayerPrefs lost (a restored
// backup, another machine) is put back into PlayerPrefs from the save. The client's own settings (Options.*, Hud.*) live
// only in save.json.
//
// Migration: the first start with the save system lists the PlayerPrefs keys of this build (macOS: the preferences plist via
// plutil; Windows: the registry; Linux: the prefs file) plus the known keys, reads each with its type and writes them into
// save.json after a "before the save system" backup. PlayerPrefs itself is left as it was. Recorded in "migrated" per store
// (the editor's and a built game's are separate).
//
// Automated runs (batch mode) read the player's save but never write it (the values they set live in memory; the old keys
// still go to PlayerPrefs as before, which the tests restore). -reflexSaveDir <dir> (or REFLEX_SAVE_DIR) gives a run its own
// save folder, which it does write (the tests of this file: Editor/Options/OptionsTests.cs).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Options
{
    public static class ReflexSave
    {
        public const string Format = "reflex-client-save";
        public const int Version = 1;
        const int AutoBackupsKept = 10;

        static readonly object s_lock = new object();
        static Dictionary<string, int> s_ints = new Dictionary<string, int>(StringComparer.Ordinal);
        static Dictionary<string, float> s_floats = new Dictionary<string, float>(StringComparer.Ordinal);
        static Dictionary<string, string> s_strings = new Dictionary<string, string>(StringComparer.Ordinal);
        static HashSet<string> s_deleted = new HashSet<string>(StringComparer.Ordinal);
        static readonly HashSet<string> s_dirty = new HashSet<string>(StringComparer.Ordinal);
        static HashSet<string> s_migrated = new HashSet<string>(StringComparer.Ordinal);
        static bool s_loaded, s_writable, s_metaDirty;
        static DateTime s_fileTime;
        static string s_folder, s_lastAutoBackup;
        static float s_dirtySince = -1;
        static int s_mainThread = -1;

        /// The old PlayerPrefs keys the client's code and tests know (the migration also lists the store itself).
        public static readonly string[] KnownLegacyKeys =
        {
            "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode", "ReflexOriginalHud", "ReflexClientInfo", "ReflexNativeGame",
            "ReflexKeyHints", "ReflexProfileName", "ReflexGameDir",
            "ReflexUI.DrivingCamera", "ReflexUI.EngineVolume", "ReflexUI.OnlinePlayerName", "ReflexUI.Brightness", "ReflexUI.Gamma",
            "ReflexUI.MeasurementSystem", "ReflexUI.CareerDifficulty", "ReflexUI.Vibration", "ReflexUI.MouseSensitivity",
            "ReflexAudio.master", "ReflexAudio.music", "ReflexAudio.musicOn",
            "ReflexFpv.mode", "ReflexFpv.fov", "ReflexFpv.tilt", "ReflexFpv.pitch", "ReflexFpv.ca", "ReflexFpv.caScale", "ReflexFpv.leanHead",
            "ReflexFpv.free", "ReflexFpv.shift", "ReflexFpv.shiftStrength",
            "ReflexFpv.leanPitch", "ReflexFpv.offX", "ReflexFpv.offY", "ReflexFpv.offZ", "ReflexFpv.trueSmooth", "ReflexFpv.trueRoll", "ReflexFpv.trueYaw", "ReflexFpv.truePitch", "ReflexFpv.shiftLong", "ReflexFpv.shiftLat", "ReflexFpv.shiftVert",
            "ReflexFpv.headBob", "ReflexFpv.headSmooth", "ReflexFpv.viewSmooth",   // (w-fpvrevert)
        };

        /// An old key: kept in PlayerPrefs (the original front end and the tests read it there), mirrored in the save.
        public static bool IsLegacy(string key) => key != null && key.StartsWith("Reflex", StringComparison.Ordinal);

        static string Arg(string name)
        {
            var a = Environment.GetCommandLineArgs(); int i = Array.IndexOf(a, name);
            if (i >= 0 && i + 1 < a.Length) return a[i + 1];
            string e = Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR");
            return string.IsNullOrEmpty(e) ? null : e;
        }
        /// -reflexSaveDir / REFLEX_SAVE_DIR: a run's own save folder (written, even in batch mode).
        public static string DirArg => s_dirArg ??= Arg("-reflexSaveDir") ?? "";
        static string s_dirArg;
        static bool HasDirArg => DirArg.Length > 0;
        /// A batch-mode run without its own save folder: reads the player's save, writes nothing to it.
        public static bool Sandboxed => Application.isBatchMode && !HasDirArg;

        public static string Folder { get { Load(); return s_folder; } }
        public static string FilePath => Path.Combine(Folder, "save.json");
        public static string BackupFolder => Path.Combine(Folder, "Backups");
        static string PendingRestore => Path.Combine(s_folder, "restore-pending.zip");
        /// The folder whose ReflexCache/ (the game's own save, graphics.cfg) the backups hold: persistentDataPath, or the run's own folder.
        static string DataRoot => HasDirArg ? s_folder : Application.persistentDataPath;
        /// How this start's save came to be: "loaded", "migrated" (from PlayerPrefs this start), "new".
        public static string Origin { get; private set; } = "";
        public static int MigratedKeys { get; private set; }
        /// The backup a pending restore came from this start (null: none).
        public static string RestoredThisStart { get; private set; }
        public static bool Writable { get { Load(); return s_writable; } }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Boot() { s_mainThread = System.Threading.Thread.CurrentThread.ManagedThreadId; Application.quitting -= Flush; Application.quitting += Flush; }

        static bool MainThread => s_mainThread < 0 || System.Threading.Thread.CurrentThread.ManagedThreadId == s_mainThread;

        /// Reads the save (main thread, the first time), applying a pending restore and migrating PlayerPrefs on the first start.
        public static void Load()
        {
            if (s_loaded) return;
            bool pump;
            lock (s_lock)
            {
                if (s_loaded) return;
                if (s_mainThread < 0) s_mainThread = System.Threading.Thread.CurrentThread.ManagedThreadId;
                s_folder = HasDirArg ? Path.GetFullPath(DirArg) : Path.Combine(Application.persistentDataPath, "Save");
                s_writable = !Sandboxed;
                s_loaded = true;
                bool restored = false;
                try
                {
                    if (s_writable) { Directory.CreateDirectory(s_folder); restored = ApplyPendingRestore(); }
                    if (File.Exists(FilePath) && Read(FilePath, true)) Origin = "loaded";
                    else if (File.Exists(FilePath + ".bak") && Read(FilePath + ".bak", true)) { Origin = "loaded"; Debug.LogWarning("[Save] save.json unreadable, using save.json.bak"); }
                    else Origin = "new";
                    if (s_writable && !s_migrated.Contains(Source)) Migrate();
                    if (s_writable) PushLegacy(restored);
                }
                catch (Exception e) { Debug.LogException(e); }
                pump = s_writable && Application.isPlaying;
            }
            if (pump) Pump.Start();
            if (s_writable && Origin != "migrated") AutoBackup();
        }

        /// (Tests) forgets the loaded save: the next access loads it again, as a new start would.
        public static void ReloadForTest()
        {
            lock (s_lock)
            {
                s_loaded = false; s_ints.Clear(); s_floats.Clear(); s_strings.Clear(); s_deleted.Clear(); s_dirty.Clear();
                s_metaDirty = false; s_lastAutoBackup = null; s_migrated = new HashSet<string>(StringComparer.Ordinal); Origin = ""; MigratedKeys = 0;
                RestoredThisStart = null; s_dirtySince = -1;
            }
        }

        // ------------------------------------------------------------------------------------------------ values

        public static bool TryGetInt(string key, out int v)
        {
            Load();
            if (LegacyLive(key, out bool has)) { v = 0; if (!has) return false; v = PlayerPrefs.GetInt(key, 0); lock (s_lock) Mirror(key, 'i', v, 0, null); return true; }
            lock (s_lock) return s_ints.TryGetValue(key, out v);
        }
        public static bool TryGetFloat(string key, out float v)
        {
            Load();
            if (LegacyLive(key, out bool has)) { v = 0; if (!has) return false; v = PlayerPrefs.GetFloat(key, 0f); lock (s_lock) Mirror(key, 'f', 0, v, null); return true; }
            lock (s_lock) return s_floats.TryGetValue(key, out v);
        }
        public static bool TryGetString(string key, out string v)
        {
            Load();
            if (LegacyLive(key, out bool has)) { v = null; if (!has) return false; v = PlayerPrefs.GetString(key, ""); lock (s_lock) Mirror(key, 's', 0, 0, v); return true; }
            lock (s_lock) return s_strings.TryGetValue(key, out v);
        }
        public static bool Has(string key)
        {
            Load();
            if (LegacyLive(key, out bool has)) return has;
            lock (s_lock) return s_ints.ContainsKey(key) || s_floats.ContainsKey(key) || s_strings.ContainsKey(key);
        }

        public static int GetInt(string key, int def = 0) => TryGetInt(key, out int v) ? v : def;
        public static float GetFloat(string key, float def = 0f) => TryGetFloat(key, out float v) ? v : def;
        public static string GetString(string key, string def = "") => TryGetString(key, out string v) ? v : def;
        public static bool GetBool(string key, bool def = false) => TryGetInt(key, out int v) ? v != 0 : def;

        public static void SetInt(string key, int v) { Load(); if (IsLegacy(key) && MainThread) PlayerPrefs.SetInt(key, v); lock (s_lock) { Clear(key); s_ints[key] = v; Touch(key); } Changed?.Invoke(key); }
        public static void SetFloat(string key, float v) { Load(); if (IsLegacy(key) && MainThread) PlayerPrefs.SetFloat(key, v); lock (s_lock) { Clear(key); s_floats[key] = v; Touch(key); } Changed?.Invoke(key); }
        public static void SetString(string key, string v) { Load(); if (IsLegacy(key) && MainThread) PlayerPrefs.SetString(key, v ?? ""); lock (s_lock) { Clear(key); s_strings[key] = v ?? ""; Touch(key); } Changed?.Invoke(key); }
        public static void SetBool(string key, bool v) => SetInt(key, v ? 1 : 0);
        public static void Delete(string key) { Load(); if (IsLegacy(key) && MainThread) PlayerPrefs.DeleteKey(key); lock (s_lock) { Clear(key); s_deleted.Add(key); Touch(key); } Changed?.Invoke(key); }

        /// A value changed through this API (the key).
        public static event Action<string> Changed;

        /// Writes now: PlayerPrefs (the old keys) and the file (it also writes itself a second after a change and at quit).
        public static void Save() { if (MainThread) try { PlayerPrefs.Save(); } catch (Exception) { } Flush(); }

        /// Every key (sorted).
        public static List<string> Keys() { Load(); lock (s_lock) return s_ints.Keys.Concat(s_floats.Keys).Concat(s_strings.Keys).Distinct().OrderBy(k => k, StringComparer.Ordinal).ToList(); }

        static void Clear(string key) { s_ints.Remove(key); s_floats.Remove(key); s_strings.Remove(key); s_deleted.Remove(key); }
        static void Touch(string key) { s_dirty.Add(key); if (s_dirtySince < 0) s_dirtySince = Now; }
        static float Now { get { try { return Time.realtimeSinceStartup; } catch (Exception) { return 0; } } }

        // an old key, on the main thread: PlayerPrefs is the live store (has = PlayerPrefs has it)
        static bool LegacyLive(string key, out bool has)
        {
            has = false;
            if (!IsLegacy(key) || !MainThread) return false;
            try { has = PlayerPrefs.HasKey(key); }
            catch (Exception) { return false; }
            if (!has) lock (s_lock) if (s_ints.ContainsKey(key) || s_floats.ContainsKey(key) || s_strings.ContainsKey(key)) { Clear(key); s_deleted.Add(key); Touch(key); }
            return true;
        }

        // the save's copy of an old key follows PlayerPrefs' value (marked to write when it changed)
        static void Mirror(string key, char kind, int i, float f, string s)
        {
            switch (kind)
            {
                case 'i': if (s_ints.TryGetValue(key, out int oi) && oi == i) return; Clear(key); s_ints[key] = i; break;
                case 'f': if (s_floats.TryGetValue(key, out float of) && of.Equals(f)) return; Clear(key); s_floats[key] = f; break;
                default: if (s_strings.TryGetValue(key, out string os) && os == s) return; Clear(key); s_strings[key] = s ?? ""; break;
            }
            Touch(key);
        }

        // ------------------------------------------------------------------------------------------------ the file

        static bool Read(string path, bool setTime)
        {
            try
            {
                var root = Json.Parse(File.ReadAllText(path, Encoding.UTF8)) as Dictionary<string, object>;
                if (root == null || !(root.TryGetValue("format", out var f) && f as string == Format)) { Debug.LogWarning($"[Save] {path} is not a save file"); return false; }
                var ints = new Dictionary<string, int>(StringComparer.Ordinal); var floats = new Dictionary<string, float>(StringComparer.Ordinal);
                var strings = new Dictionary<string, string>(StringComparer.Ordinal); var deleted = new HashSet<string>(StringComparer.Ordinal);
                if (root.TryGetValue("ints", out var io) && io is Dictionary<string, object> id) foreach (var kv in id) if (kv.Value is double d) ints[kv.Key] = (int)d;
                if (root.TryGetValue("floats", out var fo) && fo is Dictionary<string, object> fd) foreach (var kv in fd) if (kv.Value is double d) floats[kv.Key] = (float)d;
                if (root.TryGetValue("strings", out var so) && so is Dictionary<string, object> sd) foreach (var kv in sd) if (kv.Value is string s) strings[kv.Key] = s;
                if (root.TryGetValue("deleted", out var dl) && dl is List<object> l) foreach (var k in l) if (k is string s) deleted.Add(s);
                s_lastAutoBackup = root.TryGetValue("lastAutoBackup", out var lb) ? lb as string : null;
                var migrated = new HashSet<string>(StringComparer.Ordinal);
                if (root.TryGetValue("migrated", out var mg) && mg is List<object> ml) foreach (var m in ml) if (m is string ms) migrated.Add(ms);
                s_migrated = migrated; s_ints = ints; s_floats = floats; s_strings = strings; s_deleted = deleted;
                if (setTime) s_fileTime = File.GetLastWriteTimeUtc(path);
                return true;
            }
            catch (Exception e) { Debug.LogWarning($"[Save] {path} unreadable ({e.Message})"); return false; }
        }

        static string Serialize()
        {
            var ints = new SortedDictionary<string, object>(StringComparer.Ordinal); var floats = new SortedDictionary<string, object>(StringComparer.Ordinal);
            var strings = new SortedDictionary<string, object>(StringComparer.Ordinal);
            foreach (var kv in s_ints) ints[kv.Key] = kv.Value;
            foreach (var kv in s_floats) floats[kv.Key] = kv.Value;
            foreach (var kv in s_strings) strings[kv.Key] = kv.Value;
            string game = ""; try { game = Application.version; } catch (Exception) { }
            var root = new List<KeyValuePair<string, object>>
            {
                new KeyValuePair<string, object>("format", Format), new KeyValuePair<string, object>("version", Version),
                new KeyValuePair<string, object>("game", game),
                new KeyValuePair<string, object>("saved", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture)),
                new KeyValuePair<string, object>("ints", ints), new KeyValuePair<string, object>("floats", floats), new KeyValuePair<string, object>("strings", strings),
            };
            if (s_deleted.Count > 0) root.Add(new KeyValuePair<string, object>("deleted", s_deleted.OrderBy(k => k, StringComparer.Ordinal).Cast<object>().ToList()));
            if (!string.IsNullOrEmpty(s_lastAutoBackup)) root.Add(new KeyValuePair<string, object>("lastAutoBackup", s_lastAutoBackup));
            if (s_migrated.Count > 0) root.Add(new KeyValuePair<string, object>("migrated", s_migrated.OrderBy(k => k, StringComparer.Ordinal).Cast<object>().ToList()));
            return Json.Write(root);
        }

        /// Writes the changes (Save, a second after a change, on losing the focus, at quit). The old keys are first taken from
        /// PlayerPrefs (what the original front end wrote meanwhile); another process's newer file is merged (its values stay
        /// except the keys this process changed).
        public static void Flush()
        {
            if (!s_loaded || !s_writable) return;
            lock (s_lock)
            {
                if (MainThread) SyncLegacy();
                if (s_dirty.Count == 0 && !s_metaDirty && File.Exists(FilePath)) return;
                try
                {
                    if (File.Exists(FilePath) && File.GetLastWriteTimeUtc(FilePath) != s_fileTime) Merge();
                    Directory.CreateDirectory(s_folder);
                    string tmp = FilePath + ".tmp";
                    File.WriteAllText(tmp, Serialize(), new UTF8Encoding(false));
                    if (File.Exists(FilePath)) File.Copy(FilePath, FilePath + ".bak", true);
                    File.Copy(tmp, FilePath, true); File.Delete(tmp);
                    s_fileTime = File.GetLastWriteTimeUtc(FilePath);
                    s_dirty.Clear(); s_metaDirty = false; s_dirtySince = -1;
                }
                catch (Exception e) { Debug.LogWarning("[Save] write failed: " + e.Message); }
            }
        }

        // every old key the save or the client knows, as PlayerPrefs has it now
        static void SyncLegacy()
        {
            var keys = new HashSet<string>(KnownLegacyKeys, StringComparer.Ordinal);
            foreach (var k in s_ints.Keys) if (IsLegacy(k)) keys.Add(k);
            foreach (var k in s_floats.Keys) if (IsLegacy(k)) keys.Add(k);
            foreach (var k in s_strings.Keys) if (IsLegacy(k)) keys.Add(k);
            foreach (var k in keys)
            {
                bool has; try { has = PlayerPrefs.HasKey(k); } catch (Exception) { return; }
                bool mine = s_ints.ContainsKey(k) || s_floats.ContainsKey(k) || s_strings.ContainsKey(k);
                if (!has) { if (mine) { Clear(k); s_deleted.Add(k); Touch(k); } continue; }
                if (s_ints.ContainsKey(k)) Mirror(k, 'i', PlayerPrefs.GetInt(k, 0), 0, null);
                else if (s_floats.ContainsKey(k)) Mirror(k, 'f', 0, PlayerPrefs.GetFloat(k, 0f), null);
                else if (s_strings.ContainsKey(k)) Mirror(k, 's', 0, 0, PlayerPrefs.GetString(k, ""));
                else if (ImportKey(k, null)) Touch(k);
            }
        }

        static void Merge()
        {
            var mine = new Dictionary<string, (int t, object v)>(StringComparer.Ordinal);
            foreach (var k in s_dirty)
                mine[k] = s_ints.TryGetValue(k, out int i) ? (0, i) : s_floats.TryGetValue(k, out float f) ? (1, f) : s_strings.TryGetValue(k, out string s) ? (2, (object)s) : (3, null);
            string lastAuto = s_lastAutoBackup; var migrated = s_migrated;
            if (!Read(FilePath, true)) return;
            s_migrated.UnionWith(migrated);
            if (string.CompareOrdinal(lastAuto ?? "", s_lastAutoBackup ?? "") > 0) s_lastAutoBackup = lastAuto;
            foreach (var kv in mine)
            {
                Clear(kv.Key);
                switch (kv.Value.t) { case 0: s_ints[kv.Key] = (int)kv.Value.v; break; case 1: s_floats[kv.Key] = (float)kv.Value.v; break; case 2: s_strings[kv.Key] = (string)kv.Value.v; break; default: s_deleted.Add(kv.Key); break; }
            }
        }

        /// Flushes a second after the last change, on losing the focus and on pausing (play mode).
        sealed class Pump : MonoBehaviour
        {
            static Pump s_inst;
            public static void Start()
            {
                if (s_inst) return;
                var go = new GameObject("ReflexSave") { hideFlags = HideFlags.HideAndDontSave };
                DontDestroyOnLoad(go); s_inst = go.AddComponent<Pump>();
            }
            void Update() { if (s_dirtySince >= 0 && Time.realtimeSinceStartup - s_dirtySince > 1f) Save(); }
            void OnApplicationPause(bool paused) { if (paused) Save(); }
            void OnApplicationFocus(bool focused) { if (!focused) Save(); }
        }

        // the start: an old key PlayerPrefs does not have comes back from the save (all of them after a restore, which wins)
        static void PushLegacy(bool restored)
        {
            if (!MainThread) return;
            try
            {
                int n = 0;
                foreach (var kv in s_ints) if (IsLegacy(kv.Key) && (restored || !PlayerPrefs.HasKey(kv.Key))) { PlayerPrefs.SetInt(kv.Key, kv.Value); n++; }
                foreach (var kv in s_floats) if (IsLegacy(kv.Key) && (restored || !PlayerPrefs.HasKey(kv.Key))) { PlayerPrefs.SetFloat(kv.Key, kv.Value); n++; }
                foreach (var kv in s_strings) if (IsLegacy(kv.Key) && (restored || !PlayerPrefs.HasKey(kv.Key))) { PlayerPrefs.SetString(kv.Key, kv.Value); n++; }
                if (restored) foreach (var k in s_deleted) if (IsLegacy(k) && PlayerPrefs.HasKey(k)) { PlayerPrefs.DeleteKey(k); n++; }
                if (n > 0) { PlayerPrefs.Save(); Debug.Log($"[Save] {n} settings put back into PlayerPrefs from the save{(restored ? " (restored backup)" : "")}"); }
            }
            catch (Exception e) { Debug.LogWarning("[Save] PlayerPrefs not updated: " + e.Message); }
        }

        // ------------------------------------------------------------------------------------------------ the migration

        /// Unity's own PlayerPrefs (screen / quality): they stay in PlayerPrefs.
        static bool IsUnityKey(string k) => k.StartsWith("Screenmanager", StringComparison.Ordinal) || k.StartsWith("UnityGraphicsQuality", StringComparison.Ordinal) ||
                                             k.StartsWith("unity.", StringComparison.Ordinal) || k.StartsWith("UnitySelectMonitor", StringComparison.Ordinal);

        /// This process's PlayerPrefs store: a built game's or the editor's.
        static string Source => (Application.isEditor ? "editor:" : "player:") + Application.identifier + (HasDirArg ? ":" + Application.productName : "");

        static void Migrate()
        {
            var listed = ListPlayerPrefs(out string how);
            var keys = new Dictionary<string, char?>(StringComparer.Ordinal);
            foreach (var (key, kind) in listed) keys[key] = kind;
            foreach (var k in KnownLegacyKeys) if (!keys.ContainsKey(k)) keys[k] = null;
            bool win = !Application.isEditor;
            int n = 0;
            foreach (var kv in keys)
            {
                string key = kv.Key;
                if (IsUnityKey(key) || s_deleted.Contains(key)) continue;
                try { if (!PlayerPrefs.HasKey(key)) continue; } catch (Exception) { continue; }
                bool had = s_ints.ContainsKey(key) || s_floats.ContainsKey(key) || s_strings.ContainsKey(key);
                if (had && !win && !IsLegacy(key)) continue;
                if (had) Clear(key);
                if (ImportKey(key, kv.Value)) { n++; s_dirty.Add(key); }
            }
            MigratedKeys = n;
            if (n > 0)
            {
                Origin = "migrated";
                try { BackUpLocked("before the save system", ("playerprefs.json", Serialize())); } catch (Exception e) { Debug.LogWarning("[Save] pre-migration backup failed: " + e.Message); }
            }
            s_migrated.Add(Source); s_metaDirty = true;
            FlushLocked();
            Debug.Log($"[Save] {(n > 0 ? $"moved {n} settings from PlayerPrefs ({how}, {Source})" : $"nothing to move from PlayerPrefs ({Source})")} -> {FilePath}");
        }

        static void FlushLocked()
        {
            try
            {
                Directory.CreateDirectory(s_folder);
                string tmp = FilePath + ".tmp";
                File.WriteAllText(tmp, Serialize(), new UTF8Encoding(false));
                if (File.Exists(FilePath)) File.Copy(FilePath, FilePath + ".bak", true);
                File.Copy(tmp, FilePath, true); File.Delete(tmp);
                s_fileTime = File.GetLastWriteTimeUtc(FilePath);
                s_dirty.Clear(); s_metaDirty = false; s_dirtySince = -1;
            }
            catch (Exception e) { Debug.LogWarning("[Save] write failed: " + e.Message); }
        }

        /// One PlayerPrefs key into the save; kind 'i' / 'f' / 's' when the listing knows it, else found by asking.
        static bool ImportKey(string key, char? kind)
        {
            const int SentI = -2147480123; const float SentF = -3.0e38f; const string SentS = "\u0001reflex-none\u0001";
            try
            {
                if (kind == 'i' || kind == null) { int a = PlayerPrefs.GetInt(key, SentI); if (a != SentI || PlayerPrefs.GetInt(key, SentI + 1) == SentI) { s_ints[key] = a; return true; } }
                if (kind == 'f' || kind == null) { float a = PlayerPrefs.GetFloat(key, SentF); if (a != SentF) { s_floats[key] = a; return true; } }
                if (kind == 's' || kind == null) { string a = PlayerPrefs.GetString(key, SentS); if (a != SentS) { s_strings[key] = a; return true; } }
                if (kind != null) return ImportKey(key, null);
            }
            catch (Exception e) { Debug.LogWarning($"[Save] PlayerPrefs '{key}': {e.Message}"); }
            return false;
        }

        static List<(string key, char? kind)> ListPlayerPrefs(out string how)
        {
            var list = new List<(string, char?)>();
            how = "known keys";
            try
            {
                string company = Application.companyName, product = Application.productName;
                if (Application.platform == RuntimePlatform.OSXPlayer || Application.platform == RuntimePlatform.OSXEditor)
                {
                    string name = Application.isEditor ? $"unity.{company}.{product}" : Application.identifier;
                    string plist = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Library/Preferences", name + ".plist");
                    if (File.Exists(plist)) { ListPlist(plist, list); how = "macOS preferences"; }
                }
                else if (Application.platform == RuntimePlatform.WindowsPlayer || Application.platform == RuntimePlatform.WindowsEditor)
                {
                    ListRegistry(Application.isEditor ? $@"Software\Unity\UnityEditor\{company}\{product}" : $@"Software\{company}\{product}", list);
                    how = "the Windows registry";
                }
                else if (Application.platform == RuntimePlatform.LinuxPlayer || Application.platform == RuntimePlatform.LinuxEditor)
                {
                    string f = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".config/unity3d", company, product, "prefs");
                    if (File.Exists(f)) { ListLinux(f, list); how = "the Linux prefs file"; }
                }
            }
            catch (Exception e) { Debug.LogWarning("[Save] listing PlayerPrefs failed (the known keys still come over): " + e.Message); }
            return list;
        }

        static void ListPlist(string plist, List<(string, char?)> list)
        {
            var psi = new ProcessStartInfo("/usr/bin/plutil", $"-convert xml1 -o - \"{plist}\"") { RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true };
            string xml;
            using (var p = Process.Start(psi)) { xml = p.StandardOutput.ReadToEnd(); p.WaitForExit(10000); }
            var doc = new System.Xml.XmlDocument { XmlResolver = null };
            doc.LoadXml(xml.Replace("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">", ""));
            var dict = doc.SelectSingleNode("/plist/dict");
            if (dict == null) return;
            string key = null;
            foreach (System.Xml.XmlNode n in dict.ChildNodes)
            {
                if (n.Name == "key") { key = n.InnerText; continue; }
                if (key == null) continue;
                list.Add((key, n.Name == "integer" ? 'i' : n.Name == "real" ? 'f' : n.Name == "string" || n.Name == "data" ? 's' : (char?)null));
                key = null;
            }
        }

        static void ListLinux(string file, List<(string, char?)> list)
        {
            var doc = new System.Xml.XmlDocument { XmlResolver = null };
            doc.Load(file);
            foreach (System.Xml.XmlNode n in doc.SelectNodes("//pref"))
            {
                string name = n.Attributes?["name"]?.Value, type = n.Attributes?["type"]?.Value;
                if (name != null) list.Add((name, type == "int" ? 'i' : type == "float" ? 'f' : type == "string" ? 's' : (char?)null));
            }
        }

        [DllImport("advapi32.dll", CharSet = CharSet.Unicode)] static extern int RegOpenKeyExW(UIntPtr hKey, string subKey, int options, int samDesired, out UIntPtr result);
        [DllImport("advapi32.dll", CharSet = CharSet.Unicode)] static extern int RegEnumValueW(UIntPtr hKey, int index, StringBuilder name, ref int nameLen, IntPtr reserved, out int type, IntPtr data, IntPtr dataLen);
        [DllImport("advapi32.dll")] static extern int RegCloseKey(UIntPtr hKey);

        static void ListRegistry(string path, List<(string, char?)> list)
        {
            var HKCU = new UIntPtr(0x80000001u);
            if (RegOpenKeyExW(HKCU, path, 0, 0x20019, out var key) != 0) return;
            try
            {
                for (int i = 0; ; i++)
                {
                    var sb = new StringBuilder(16384); int len = sb.Capacity;
                    if (RegEnumValueW(key, i, sb, ref len, IntPtr.Zero, out int type, IntPtr.Zero, IntPtr.Zero) != 0) break;
                    string name = sb.ToString(0, len);
                    int h = name.LastIndexOf("_h", StringComparison.Ordinal);
                    if (h > 0 && name.Substring(h + 2).All(char.IsDigit)) name = name.Substring(0, h);
                    list.Add((name, type == 4 ? 'i' : type == 11 ? 'f' : type == 3 ? 's' : (char?)null));
                }
            }
            finally { RegCloseKey(key); }
        }

        // ------------------------------------------------------------------------------------------------ backups

        /// A backup zip in Save/Backups (the save, the game's own save, graphics.cfg). Returns its path.
        public static string BackUp(string label)
        {
            Load();
            if (!s_writable) throw new InvalidOperationException("this run does not write the save (batch mode without -reflexSaveDir)");
            Save();
            lock (s_lock) return BackUpLocked(label, null);
        }

        static string BackUpLocked(string label, (string name, string text)? extra)
        {
            Directory.CreateDirectory(BackupFolder);
            string stamp = DateTime.Now.ToString("yyyy-MM-dd HH.mm.ss", CultureInfo.InvariantCulture);
            string safe = new string((label ?? "backup").Select(c => char.IsLetterOrDigit(c) || c == ' ' || c == '-' ? c : '_').ToArray()).Trim();
            string path = Path.Combine(BackupFolder, $"{stamp} {safe}.zip");
            for (int i = 2; File.Exists(path); i++) path = Path.Combine(BackupFolder, $"{stamp} {safe} {i}.zip");
            using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
            {
                var e = zip.CreateEntry("save.json");
                using (var w = new StreamWriter(e.Open(), new UTF8Encoding(false))) w.Write(Serialize());
                string cache = Path.Combine(DataRoot, "ReflexCache");
                string gameSave = Path.Combine(cache, "save");
                if (Directory.Exists(gameSave))
                    foreach (var f in Directory.GetFiles(gameSave, "*", SearchOption.AllDirectories))
                        zip.CreateEntryFromFile(f, "ReflexCache/save/" + f.Substring(gameSave.Length + 1).Replace(Path.DirectorySeparatorChar, '/'));
                string cfg = Path.Combine(cache, "graphics.cfg");
                if (File.Exists(cfg)) zip.CreateEntryFromFile(cfg, "ReflexCache/graphics.cfg");
                if (extra.HasValue) { var x = zip.CreateEntry(extra.Value.name); using var w = new StreamWriter(x.Open(), new UTF8Encoding(false)); w.Write(extra.Value.text); }
            }
            Debug.Log("[Save] backup " + path);
            return path;
        }

        /// The backups, newest first.
        public static List<string> Backups()
        {
            try { return Directory.Exists(BackupFolder) ? Directory.GetFiles(BackupFolder, "*.zip").OrderByDescending(f => File.GetLastWriteTimeUtc(f)).ThenByDescending(f => f, StringComparer.Ordinal).ToList() : new List<string>(); }
            catch (Exception) { return new List<string>(); }
        }

        /// One backup a day; the newest AutoBackupsKept automatic ones are kept (the others stay).
        static void AutoBackup()
        {
            string today = DateTime.Now.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
            if (s_lastAutoBackup == today || !File.Exists(FilePath)) return;
            try
            {
                lock (s_lock) { s_lastAutoBackup = today; s_metaDirty = true; BackUpLocked("auto", null); }
                foreach (var old in Backups().Where(f => f.EndsWith(" auto.zip", StringComparison.Ordinal)).Skip(AutoBackupsKept)) File.Delete(old);
                Flush();
            }
            catch (Exception e) { Debug.LogWarning("[Save] automatic backup failed: " + e.Message); }
        }

        /// Restores a backup at the next start (the game should close after this). False when the zip holds no save.
        public static bool Restore(string zipPath)
        {
            Load();
            if (!s_writable) return false;
            using (var zip = ZipFile.OpenRead(zipPath)) if (zip.GetEntry("save.json") == null) return false;
            BackUp("before restore");
            File.Copy(zipPath, PendingRestore, true);
            lock (s_lock) s_writable = false;   // nothing this session writes may land over it
            return true;
        }

        /// A restore is waiting for the next start.
        public static bool RestorePending { get { Load(); return File.Exists(PendingRestore); } }

        static bool ApplyPendingRestore()
        {
            string p = PendingRestore;
            if (!File.Exists(p)) return false;
            try
            {
                using (var zip = ZipFile.OpenRead(p))
                    foreach (var e in zip.Entries)
                    {
                        if (string.IsNullOrEmpty(e.Name) || e.FullName.Contains("..") || e.FullName == "playerprefs.json") continue;
                        string dest = e.FullName == "save.json" ? FilePath : Path.Combine(DataRoot, e.FullName.Replace('/', Path.DirectorySeparatorChar));
                        Directory.CreateDirectory(Path.GetDirectoryName(dest));
                        if (dest == FilePath && File.Exists(dest)) File.Copy(dest, dest + ".bak", true);
                        e.ExtractToFile(dest, true);
                    }
                File.Delete(p);
                RestoredThisStart = p;
                Debug.Log("[Save] restored the backup");
                return true;
            }
            catch (Exception e) { Debug.LogWarning("[Save] restoring the backup failed: " + e.Message); return false; }
        }

        /// Shows a file or folder in Finder / Explorer.
        public static void Reveal(string path)
        {
            try
            {
                if (Application.platform == RuntimePlatform.OSXPlayer || Application.platform == RuntimePlatform.OSXEditor)
                    Process.Start("/usr/bin/open", File.Exists(path) ? $"-R \"{path}\"" : $"\"{path}\"");
                else if (Application.platform == RuntimePlatform.WindowsPlayer || Application.platform == RuntimePlatform.WindowsEditor)
                    Process.Start("explorer.exe", File.Exists(path) ? $"/select,\"{path.Replace('/', '\\')}\"" : $"\"{path.Replace('/', '\\')}\"");
                else Application.OpenURL("file://" + (File.Exists(path) ? Path.GetDirectoryName(path) : path));
            }
            catch (Exception e) { Debug.LogWarning("[Save] reveal: " + e.Message); }
        }

        /// A path with the home folder as ~ (shown on screen).
        public static string Redact(string path)
        {
            if (string.IsNullOrEmpty(path)) return "";
            string home = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
            return !string.IsNullOrEmpty(home) && path.StartsWith(home, StringComparison.Ordinal) ? "~" + path.Substring(home.Length) : path;
        }

        // ------------------------------------------------------------------------------------------------ JSON

        /// A small JSON reader / writer (objects as Dictionary<string, object>, arrays List<object>, numbers double).
        public static class Json
        {
            public static object Parse(string s) { int i = 0; var v = Value(s, ref i); return v; }

            static void Ws(string s, ref int i) { while (i < s.Length && char.IsWhiteSpace(s[i])) i++; }
            static object Value(string s, ref int i)
            {
                Ws(s, ref i);
                if (i >= s.Length) throw new FormatException("end of input");
                char c = s[i];
                if (c == '{')
                {
                    var d = new Dictionary<string, object>(StringComparer.Ordinal); i++;
                    Ws(s, ref i); if (i < s.Length && s[i] == '}') { i++; return d; }
                    while (true)
                    {
                        Ws(s, ref i); string k = Str(s, ref i); Ws(s, ref i);
                        if (i >= s.Length || s[i] != ':') throw new FormatException("':' expected at " + i);
                        i++; d[k] = Value(s, ref i); Ws(s, ref i);
                        if (i < s.Length && s[i] == ',') { i++; continue; }
                        if (i < s.Length && s[i] == '}') { i++; return d; }
                        throw new FormatException("',' or '}' expected at " + i);
                    }
                }
                if (c == '[')
                {
                    var l = new List<object>(); i++;
                    Ws(s, ref i); if (i < s.Length && s[i] == ']') { i++; return l; }
                    while (true)
                    {
                        l.Add(Value(s, ref i)); Ws(s, ref i);
                        if (i < s.Length && s[i] == ',') { i++; continue; }
                        if (i < s.Length && s[i] == ']') { i++; return l; }
                        throw new FormatException("',' or ']' expected at " + i);
                    }
                }
                if (c == '"') return Str(s, ref i);
                if (s.Length - i >= 4 && string.CompareOrdinal(s, i, "true", 0, 4) == 0) { i += 4; return true; }
                if (s.Length - i >= 5 && string.CompareOrdinal(s, i, "false", 0, 5) == 0) { i += 5; return false; }
                if (s.Length - i >= 4 && string.CompareOrdinal(s, i, "null", 0, 4) == 0) { i += 4; return null; }
                int st = i;
                while (i < s.Length && (char.IsDigit(s[i]) || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) i++;
                if (i == st) throw new FormatException("value expected at " + i);
                return double.Parse(s.Substring(st, i - st), NumberStyles.Float, CultureInfo.InvariantCulture);
            }
            static string Str(string s, ref int i)
            {
                if (i >= s.Length || s[i] != '"') throw new FormatException("string expected at " + i);
                i++; var sb = new StringBuilder();
                while (i < s.Length && s[i] != '"')
                {
                    char c = s[i++];
                    if (c != '\\') { sb.Append(c); continue; }
                    if (i >= s.Length) break;
                    char e = s[i++];
                    switch (e)
                    {
                        case 'n': sb.Append('\n'); break; case 'r': sb.Append('\r'); break; case 't': sb.Append('\t'); break;
                        case 'b': sb.Append('\b'); break; case 'f': sb.Append('\f'); break;
                        case 'u': sb.Append((char)Convert.ToInt32(s.Substring(i, 4), 16)); i += 4; break;
                        default: sb.Append(e); break;
                    }
                }
                i++;
                return sb.ToString();
            }

            public static string Write(object v) { var sb = new StringBuilder(); W(sb, v, 0); sb.Append('\n'); return sb.ToString(); }
            static void Indent(StringBuilder sb, int n) { sb.Append('\n'); sb.Append(' ', n * 2); }
            static void W(StringBuilder sb, object v, int depth)
            {
                switch (v)
                {
                    case null: sb.Append("null"); return;
                    case string s: Quote(sb, s); return;
                    case bool b: sb.Append(b ? "true" : "false"); return;
                    case int n: sb.Append(n.ToString(CultureInfo.InvariantCulture)); return;
                    case float f: sb.Append(float.IsNaN(f) || float.IsInfinity(f) ? "0" : f.ToString("R", CultureInfo.InvariantCulture)); return;
                    case double d: sb.Append(double.IsNaN(d) || double.IsInfinity(d) ? "0" : d.ToString("R", CultureInfo.InvariantCulture)); return;
                    case IEnumerable<KeyValuePair<string, object>> obj:
                    {
                        sb.Append('{'); bool first = true;
                        foreach (var kv in obj) { if (!first) sb.Append(','); first = false; Indent(sb, depth + 1); Quote(sb, kv.Key); sb.Append(": "); W(sb, kv.Value, depth + 1); }
                        if (!first) Indent(sb, depth);
                        sb.Append('}'); return;
                    }
                    case System.Collections.IEnumerable list:
                    {
                        sb.Append('['); bool first = true;
                        foreach (var x in list) { if (!first) sb.Append(','); first = false; Indent(sb, depth + 1); W(sb, x, depth + 1); }
                        if (!first) Indent(sb, depth);
                        sb.Append(']'); return;
                    }
                    default: Quote(sb, Convert.ToString(v, CultureInfo.InvariantCulture)); return;
                }
            }
            static void Quote(StringBuilder sb, string s)
            {
                sb.Append('"');
                foreach (char c in s)
                {
                    switch (c)
                    {
                        case '"': sb.Append("\\\""); break; case '\\': sb.Append("\\\\"); break;
                        case '\n': sb.Append("\\n"); break; case '\r': sb.Append("\\r"); break; case '\t': sb.Append("\\t"); break;
                        default: if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4")); else sb.Append(c); break;
                    }
                }
                sb.Append('"');
            }
        }
    }
}

using System;
using System.IO;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    /// Native-core controls. All entry points are main-thread operations; no profile runtime or live session is opened.
    public static class CoreRemapping
    {
        [Serializable] sealed class Saved { public int version = 1; public int[] values; }
        static readonly object Gate = new object();
        public static string FilePath => Path.Combine(CoreGarageProfile.SaveDirectory, "core-controls-v1.json");
        public static string Error { get; private set; }
        static readonly string[] Columns = { "Throttle", "Brake", "SteerLeft", "SteerRight", "TrickModifier", "Clutch", "ReflexLeft", "ReflexRight", "ReflexUp", "ReflexDown", "Camera", "SetResetPoint", "Reset", "UseResetPoint", "Reverse", "CameraTweakLeft", "CameraTweakRight", "CameraTweakUp", "CameraTweakDown" };
        static readonly string[] PadNames = { "", "LEFT STICK X", "LEFT STICK Y", "RIGHT STICK X", "RIGHT STICK Y", "PAD 5", "PAD 6", "PAD 7", "PAD 8", "A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LEFT STICK CLICK", "RIGHT STICK CLICK", "LT", "RT", "DPAD UP", "DPAD RIGHT", "DPAD DOWN", "DPAD LEFT" };

        static void EnsureLoaded()
        {
            if (!string.IsNullOrEmpty(RxNativeLoader.LoadedPath)) return;
            string path = RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor));
            string project = Path.GetDirectoryName(Application.dataPath);
            RxNativeLoader.Load(path, Application.isEditor ? Path.Combine(string.IsNullOrEmpty(project) ? Application.dataPath : project, "Temp", "OpenReflex") : null);
        }
        public static void Validate(int[] values)
        {
            if (values == null || values.Length != 38) throw new FormatException("Controls must contain all 38 bindings.");
            for (int i = 0; i < values.Length; ++i)
            {
                int v = values[i]; bool key = v >= 0x23 && v <= 0x87;
                if (i < 19 ? !key : !(v >= 1 && v <= 24 || i >= 34 && key))
                    throw new FormatException("Invalid controller binding at " + i + ".");
            }
            // Defaults deliberately share paired axes. Duplicate rejection applies to UI edits, not whole-table loading.
        }
        static int[] ReadSaved()
        {
            string path = FilePath;
            if (!File.Exists(path)) return null;
            if (new FileInfo(path).Length > 16384) throw new FormatException("Controls file is too large.");
            var saved = JsonUtility.FromJson<Saved>(File.ReadAllText(path));
            if (saved == null || saved.version != 1) throw new FormatException("Unsupported controls file version.");
            Validate(saved.values); return (int[])saved.values.Clone();
        }
        /// Called before creating the simulation worker; malformed saves fail startup instead of partially applying controls.
        public static int[] StartupValues() { lock (Gate) return ReadSaved(); }
        public static unsafe RemapTable Read(bool defaults = false)
        {
            lock (Gate)
            {
                EnsureLoaded(); var native = new RxRemapTable();
                if (RxApiNative.rx_remap_defaults(ref native) != 0) throw new InvalidOperationException("Native controller defaults are unavailable.");
                var t = new RemapTable { columns = (string[])Columns.Clone(), kb = new int[19], js = new int[19], kbText = new string[19], jsText = new string[19] };
                for (int i = 0; i < 19; ++i) { t.kb[i] = native.keyboard[i]; t.js[i] = native.pad[i]; }
                for (int i = 0; i < 256; ++i) t.scan[i] = checked((byte)native.scan[i]);
                for (int i = 1; i < PadNames.Length; ++i) t.names[i] = PadNames[i];
                foreach (var key in DirectInputMap.Table)
                {
                    int id = t.scan[key.Value];
                    if (id != 0) t.names[id] = key.Key.ToString();
                }
                for (int id = 0x23; id <= 0x87; ++id) if (!t.names.ContainsKey(id)) t.names[id] = "KEY " + id;
                int[] values = defaults ? null : ReadSaved();
                if (values != null) { Array.Copy(values, 0, t.kb, 0, 19); Array.Copy(values, 19, t.js, 0, 19); }
                for (int i = 0; i < 19; ++i) { t.kbText[i] = t.Name(t.kb[i]); t.jsText[i] = t.Name(t.js[i]); }
                Error = null; return t;
            }
        }
        /// The table's 38 values (keyboard row then pad row), as the file and the core take them.
        public static int[] Values(RemapTable table)
        {
            var values = new int[38]; Array.Copy(table.kb, 0, values, 0, 19); Array.Copy(table.js, 0, values, 19, 19); return values;
        }
        /// Bumped by every Save (w-fb-hud): the readers that keep a copy of the table (the spawn markers' "taken" inputs, the hovercraft) read it again.
        public static int Generation { get; private set; }
        public static event Action BindingsSaved;

        /// Saves the player's bindings and, in a running event, applies them to it at once (w-fb-hud: the pause page's CONTROLS and the menus'
        /// are one path; the event's KeyMaps are built again by the core, rx_session_remap_apply). True when a running event took them.
        public static bool SaveAndApply(RemapTable table)
        {
            Save(table);
            var rt = ReflexClientLive.Runtime; var open = rt && rt.Running ? rt.SimDriver?.Open : null;
            if (open == null) return false;
            var values = Values(table);
            open.ApplyRemap(values, rc => { LastApplied = rc; if (rc == 0) AppliedCount++; else UnityEngine.Debug.LogWarning("[Controls] the running event did not take the bindings (" + rc + ")"); });
            UnityEngine.Debug.Log("[Controls] bindings saved and applied to the running event");
            return true;
        }
        /// The running event's last answer to SaveAndApply (tests): 0 applied; int.MinValue none yet. AppliedCount: the 0 answers.
        public static volatile int LastApplied = int.MinValue;
        public static int AppliedCount;

        public static void Save(RemapTable table)
        {
            lock (Gate)
            {
                var values = Values(table); Validate(values);
                string path = FilePath; Directory.CreateDirectory(Path.GetDirectoryName(path));
                string temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
                try
                {
                    byte[] data = Encoding.UTF8.GetBytes(JsonUtility.ToJson(new Saved { values = values }, true));
                    using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { stream.Write(data, 0, data.Length); stream.Flush(true); }
                    if (File.Exists(path)) File.Replace(temp, path, null); else File.Move(temp, path);
                    Error = null; Generation++;
                }
                catch (Exception e) { Error = e.Message; throw; }
                finally { if (File.Exists(temp)) File.Delete(temp); }
            }
            try { BindingsSaved?.Invoke(); } catch (Exception e) { UnityEngine.Debug.LogException(e); }
        }
    }
}

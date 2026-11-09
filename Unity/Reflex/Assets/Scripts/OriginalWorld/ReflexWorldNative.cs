using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.OriginalWorld
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexPad
    {
        public ushort buttons;
        public byte leftTrigger, rightTrigger;
        public short lx, ly, rx, ry;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexBikeState
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public float[] position;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public float[] orientation;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public float[] renderPosition;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public float[] renderOrientation;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] velocity;
        public float speed;
        public int gear;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public float[] inputs;
        public uint frames;
        public uint reserved;

        public static ReflexBikeState Create() => new ReflexBikeState
        {
            position = new float[16], orientation = new float[16], renderPosition = new float[16],
            renderOrientation = new float[16], velocity = new float[4], inputs = new float[8]
        };
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexTerrainInfo
    {
        public uint samples, stride;
        public float spacing, min_x, min_z, max_x, max_z, gain;
        public uint deform_version;
        public int dirty_x0, dirty_z0, dirty_x1, dirty_z1;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct ReflexAudioCommand
    {
        public int sub; public uint wrapper, serial; public float value;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public float[] position;
        public int has_position, state;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 96)] public string path;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string param;

        // The native layout (reflex_world.h) read from a raw buffer: marshalling the struct array copies back every element of it
        // (strings and arrays for all of them) on each call; this decodes only the commands returned.
        public const int NativeSize = 164;
        public static ReflexAudioCommand Decode(byte[] b, int index)
        {
            int o = index * NativeSize;
            return new ReflexAudioCommand
            {
                sub = BitConverter.ToInt32(b, o), wrapper = BitConverter.ToUInt32(b, o + 4), serial = BitConverter.ToUInt32(b, o + 8),
                value = BitConverter.ToSingle(b, o + 12),
                position = BitConverter.ToInt32(b, o + 28) != 0 ? new[] { BitConverter.ToSingle(b, o + 16), BitConverter.ToSingle(b, o + 20), BitConverter.ToSingle(b, o + 24) } : NoPosition,
                has_position = BitConverter.ToInt32(b, o + 28), state = BitConverter.ToInt32(b, o + 32),
                path = CString(b, o + 36, 96), param = CString(b, o + 132, 32),
            };
        }
        static readonly float[] NoPosition = new float[3];   // shared, never written
        // The same few event paths / parameter names every frame: interned per decoding thread (FNV-1a of the bytes), no garbage.
        [ThreadStatic] static System.Collections.Generic.Dictionary<ulong, string> interned;
        static string CString(byte[] b, int o, int max)
        {
            int n = 0; ulong h = 14695981039346656037UL;
            while (n < max && b[o + n] != 0) { h = (h ^ b[o + n]) * 1099511628211UL; n++; }
            if (n == 0) return "";
            h ^= (ulong)n << 56;
            interned ??= new System.Collections.Generic.Dictionary<ulong, string>();
            if (interned.TryGetValue(h, out var s)) return s;
            if (interned.Count > 4096) interned.Clear();
            return interned[h] = System.Text.Encoding.UTF8.GetString(b, o, n);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexHudState
    {
        public int valid, activity_state, on_track, wrong_way;
        public float speed, time_behind, time_ahead, hold_clock, hold_until, penalty;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexCamera
    {
        public float px, py, pz, rx, ry, rz, ux, uy, uz, fx, fy, fz, fov, nearPlane, farPlane;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexRaceState
    {
        public int valid, state, num_laps, laps_completed, place, riders;
        public float lap_time, last_lap, best_lap, race_time, lap_progress, start_timer, reset_timer;
        /// The open core's countdown step (rx_race_state.countdown_step: 3, 2, 1; -1 none). The legacy plugin does not write it (0).
        public int countdown_step;
    }

    // One rider of the race (reflex_world_race_drivers): its player slot, place, laps, finished flag (D+0x70) and times;
    // race_time is the driver's race clock D+0x134, which stops at its finish.
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexDriverResult
    {
        public int slot, local, place, laps_completed, finished;
        public float race_time, best_lap, last_lap;
    }

    // A rider of the race (reflex_world_slots): player slot P = G+0x7460 + slot*0x12a0 with a vehicle. type: P+0x10 (0 human,
    // 1 computer); rider_row / vehicle_row: P+0x1b8 / P+0x1b0 = "Rider List".RiderId / "Vehicle List".VehicleId (00767200).
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexSlot
    {
        public int slot, type, local, rider_row, vehicle_row;
        public uint vehicle, rider;
    }

    // The bindings of libreflexworld (reflex_world.h): delegates resolved through Platform/ReflexNativeLoader (the temp-copy loader;
    // scripts/loader-branch/convert_dllimports.py wrote them from the DllImports). Never add a [DllImport("reflexworld")]: a second
    // image of the plugin with its own statics.
    public static class ReflexWorldNative
    {
        public const string Library = "reflexworld";
        public const int ExpectedStateSize = 4 * 4 * 4 * 4 + 4 * 4 + 4 + 4 + 8 * 4 + 2 * 4; // 320
        public const int ExpectedPadSize = 12;

        public static void VerifyLayout()
        {
            int state = Marshal.SizeOf<ReflexBikeState>(), pad = Marshal.SizeOf<ReflexPad>();
            if (state != ExpectedStateSize || pad != ExpectedPadSize)
                throw new InvalidOperationException($"Native struct layout mismatch: ReflexBikeState {state} (expected {ExpectedStateSize}), ReflexPad {pad} (expected {ExpectedPadSize}).");
            var check = Check();
            if (!check.Compatible) throw new InvalidOperationException("libreflexworld does not match this build: " + check.Summary);
        }

        // ---------------------------------------------------------------------------------------------------------------- API hygiene
        // reflex_world.h "API VERSION, STRUCT SIZES, CRASH REPORTS, PERF". Check() runs once per load (the first VerifyLayout, the
        // session log's start in players) and compares the plugin's reflex_world_version() with ExpectedApiVersion and its
        // reflex_world_sizeof(name) with the managed size of every struct marshalled or decoded from raw bytes. A plugin without
        // reflex_world_version (built before 2026-10-18) is accepted with a warning; a different version or any size difference
        // makes VerifyLayout throw, so a stale plugin fails with one clear message instead of corrupt memory.

        /// The REFLEX_WORLD_API_VERSION this C# code was written against (reflex_world.h changelog).
        public const int ExpectedApiVersion = 1;

        public sealed class CheckResult
        {
            public bool Loaded;            // the plugin could be loaded at all
            public bool Versioned;         // it has reflex_world_version (false: an older, unnumbered plugin)
            public int Version = -1;
            public bool Compatible = true; // false: a version or struct size mismatch (VerifyLayout throws)
            public string Summary = "";    // one line for logs
            public readonly System.Collections.Generic.List<string> Mismatches = new System.Collections.Generic.List<string>();
        }

        static CheckResult checkResult;
        static int checkGeneration = -1;   // the ReflexNativeLoader.Generation the result belongs to (a Reload checks again)
        static readonly object checkLock = new object();
        static CheckResult Cache(CheckResult r) { checkGeneration = Reflex.Port.Platform.ReflexNativeLoader.Generation; return checkResult = r; }

        /// The managed side of every struct of reflex_world.h (C name -> managed size); the raw-decoded ones read their stride
        /// constant (private ones by reflection, so the owner's constant stays the single source).
        public static System.Collections.Generic.List<(string name, int size)> ManagedSizes()
        {
            var l = new System.Collections.Generic.List<(string, int)>();
            void M<T>(string n) { try { l.Add((n, Marshal.SizeOf<T>())); } catch (Exception) { l.Add((n, -2)); } }
            void C(string n, Type t, string field)
            {
                var f = t.GetField(field, System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
                l.Add((n, f != null ? Convert.ToInt32(f.GetRawConstantValue()) : -2));
            }
            M<ReflexPad>("ReflexPad"); M<ReflexBikeState>("ReflexBikeState"); M<Reflex.Port.Content.ReflexWorldFiles.EventSpec>("ReflexEventSpec");
            M<ReflexTerrainInfo>("ReflexTerrainInfo"); M<ReflexParticle>("ReflexParticle"); M<ReflexParticleInfo>("ReflexParticleInfo");
            C("ReflexAmbientEntity", typeof(OriginalAmbient), "EntitySize"); M<ReflexAmbientInfo>("ReflexAmbientInfo");
            M<ReflexRaceState>("ReflexRaceState"); M<ReflexDriverResult>("ReflexDriverResult"); M<ReflexCamera>("ReflexCamera");
            M<ReflexAudioCommand>("ReflexAudioCommand"); l.Add(("ReflexAudioCommand", ReflexAudioCommand.NativeSize));
            M<ReflexHudState>("ReflexHudState"); M<ReflexSlot>("ReflexSlot");
            M<Reflex.Port.Content.ReflexVehicleInfo>("ReflexVehicleInfo"); l.Add(("ReflexVehicleInfo", Reflex.Port.Content.ReflexAtvNative.InfoSize));
            M<Reflex.Port.Content.ReflexMudState>("ReflexMudState"); M<Reflex.Port.Content.ReflexBink.Info>("ReflexBinkInfo");
            C("ReflexUiNode", typeof(OriginalLiveUi), "NodeSize"); C("ReflexUiLayer", typeof(OriginalLiveUi), "LayerSize");
            C("ReflexUiAnim", typeof(OriginalLiveUi), "AnimSize"); C("ReflexModeState", typeof(OriginalLiveUi), "ModeSize");
            C("ReflexModeObject", typeof(OriginalLiveUi), "ObjectSize");
            C("ReflexProp", typeof(ReflexPropsNative), "PropSize"); M<ReflexPropInfo>("ReflexPropInfo"); C("ReflexTextureAnim", typeof(ReflexPropsNative), "TextureAnimSize");
            return l;
        }

        /// Loads the plugin (if it is not yet) and checks its version and struct sizes; cached per load (ReflexNativeLoader.Generation).
        public static CheckResult Check()
        {
            lock (checkLock)
            {
                if (checkResult != null && checkGeneration == Reflex.Port.Platform.ReflexNativeLoader.Generation) return checkResult;
                var r = new CheckResult();
                try { r.Version = reflex_world_version(); r.Versioned = true; r.Loaded = true; }
                catch (EntryPointNotFoundException) { r.Loaded = true; }
                catch (DllNotFoundException e) { r.Compatible = false; r.Summary = "plugin not found: " + e.Message; return Cache(r); }
                catch (BadImageFormatException e) { r.Compatible = false; r.Summary = "plugin not loadable: " + e.Message; return Cache(r); }
                if (!r.Versioned)
                {
                    r.Summary = $"an unnumbered plugin (no reflex_world_version; this build expects API {ExpectedApiVersion}): struct sizes not checked";
                    return Cache(r);
                }
                if (r.Version != ExpectedApiVersion) { r.Compatible = false; r.Mismatches.Add($"API version {r.Version} (expected {ExpectedApiVersion})"); }
                int checkedCount = 0;
                var unchecked_ = new System.Collections.Generic.List<string>();
                foreach (var (name, size) in ManagedSizes())
                {
                    if (size < 0) { unchecked_.Add(name); continue; }   // the managed constant moved: reported, not fatal
                    int native = reflex_world_sizeof(name);
                    checkedCount++;
                    if (native < 0) { r.Mismatches.Add($"{name}: unknown to the plugin"); r.Compatible = false; }
                    else if (size != native) { r.Mismatches.Add($"{name}: native {native}, managed {size}"); r.Compatible = false; }
                }
                r.Summary = (r.Compatible ? $"API {r.Version}, {checkedCount} struct sizes match" : $"API {r.Version}: " + string.Join("; ", r.Mismatches)) +
                            (unchecked_.Count > 0 ? " (managed size not found, unchecked: " + string.Join(", ", unchecked_) + ")" : "");
                return Cache(r);
            }
        }

        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_version__fn();
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_version__fn> reflex_world_version__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_version__fn>("reflex_world_version");
        static int reflex_world_version() => reflex_world_version__p.Get()();
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_sizeof__fn([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_sizeof__fn> reflex_world_sizeof__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_sizeof__fn>("reflex_world_sizeof");
        static int reflex_world_sizeof([MarshalAs(UnmanagedType.LPUTF8Str)] string name) => reflex_world_sizeof__p.Get()(name);

        // Crash capture and perf (crash.inc). The wrappers return false / "" with an older plugin (no export) or none at all.
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_crash_install__fn([MarshalAs(UnmanagedType.LPUTF8Str)] string dir, [MarshalAs(UnmanagedType.LPUTF8Str)] string session, [MarshalAs(UnmanagedType.LPUTF8Str)] string build);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_install__fn> reflex_world_crash_install__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_install__fn>("reflex_world_crash_install");
        static int reflex_world_crash_install([MarshalAs(UnmanagedType.LPUTF8Str)] string dir, [MarshalAs(UnmanagedType.LPUTF8Str)] string session, [MarshalAs(UnmanagedType.LPUTF8Str)] string build) => reflex_world_crash_install__p.Get()(dir, session, build);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate void reflex_world_crash_set_context__fn([MarshalAs(UnmanagedType.LPUTF8Str)] string key, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_set_context__fn> reflex_world_crash_set_context__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_set_context__fn>("reflex_world_crash_set_context");
        static void reflex_world_crash_set_context([MarshalAs(UnmanagedType.LPUTF8Str)] string key, [MarshalAs(UnmanagedType.LPUTF8Str)] string value) => reflex_world_crash_set_context__p.Get()(key, value);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate void reflex_world_crash_note__fn([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_note__fn> reflex_world_crash_note__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_note__fn>("reflex_world_crash_note");
        static void reflex_world_crash_note([MarshalAs(UnmanagedType.LPUTF8Str)] string text) => reflex_world_crash_note__p.Get()(text);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_crash_status__fn(byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_status__fn> reflex_world_crash_status__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_status__fn>("reflex_world_crash_status");
        static int reflex_world_crash_status(byte[] output, int size) => reflex_world_crash_status__p.Get()(output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_crash_test__fn(int kind);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_test__fn> reflex_world_crash_test__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_crash_test__fn>("reflex_world_crash_test");
        static int reflex_world_crash_test(int kind) => reflex_world_crash_test__p.Get()(kind);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_perf_stats__fn(byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_perf_stats__fn> reflex_world_perf_stats__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_perf_stats__fn>("reflex_world_perf_stats");
        static int reflex_world_perf_stats(byte[] output, int size) => reflex_world_perf_stats__p.Get()(output, size);

        static bool noCrashExports, noPerfExport;
        public static bool HasCrashHandler => !noCrashExports;

        /// Installs the plugin's crash handler (players: ReflexCrashLog at start); false (and the reason) without it.
        public static bool CrashInstall(string dir, string session, string build, out string status)
        {
            status = "";
            if (noCrashExports) { status = "no crash handler in this plugin"; return false; }
            try
            {
                int r = reflex_world_crash_install(dir, session, build);
                status = CrashStatus();
                return r == 0;
            }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException || e is BadImageFormatException)
            { noCrashExports = true; status = "no crash handler in this plugin (" + e.GetType().Name + ")"; return false; }
        }

        public static string CrashStatus()
        {
            if (noCrashExports) return "";
            try { var b = new byte[512]; int n = reflex_world_crash_status(b, b.Length); return System.Text.Encoding.UTF8.GetString(b, 0, Math.Max(0, Math.Min(n, b.Length - 1))); }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { noCrashExports = true; return ""; }
        }

        public static void CrashContext(string key, string value)
        {
            if (noCrashExports) return;
            try { reflex_world_crash_set_context(key, value ?? ""); }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { noCrashExports = true; }
        }

        public static void CrashNote(string text)
        {
            if (noCrashExports) return;
            try { reflex_world_crash_note(text ?? ""); }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { noCrashExports = true; }
        }

        /// Debug only (-reflexCrashTest): 1 a bad write inside the plugin, 2 abort(), 3 SIGFPE. False without the export.
        public static bool CrashTest(int kind)
        {
            if (noCrashExports) return false;
            try { reflex_world_crash_test(kind); return true; }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { noCrashExports = true; return false; }
        }

        /// reflex_world_perf_stats ("heap=.. rss=.. footprint=.. threads=..") or null with an older plugin.
        public static string PerfStats(byte[] scratch)
        {
            if (noPerfExport) return null;
            try { int n = reflex_world_perf_stats(scratch, scratch.Length); return System.Text.Encoding.UTF8.GetString(scratch, 0, Math.Max(0, Math.Min(n, scratch.Length - 1))); }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { noPerfExport = true; return null; }
        }

        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_create__fn([MarshalAs(UnmanagedType.LPUTF8Str)] string exePath, [MarshalAs(UnmanagedType.LPUTF8Str)] string crtPath, [MarshalAs(UnmanagedType.LPUTF8Str)] string worldPath, [MarshalAs(UnmanagedType.LPUTF8Str)] string deformPath);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_create__fn> reflex_world_create__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_create__fn>("reflex_world_create");
        public static IntPtr reflex_world_create([MarshalAs(UnmanagedType.LPUTF8Str)] string exePath, [MarshalAs(UnmanagedType.LPUTF8Str)] string crtPath, [MarshalAs(UnmanagedType.LPUTF8Str)] string worldPath, [MarshalAs(UnmanagedType.LPUTF8Str)] string deformPath) => reflex_world_create__p.Get()(exePath, crtPath, worldPath, deformPath);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_error__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn> reflex_world_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn>("reflex_world_error");
        static IntPtr reflex_world_error(IntPtr world) => reflex_world_error__p.Get()(world);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_frame__fn(IntPtr world, float dt, byte[] keys, ReflexPad[] pads, uint padMask);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_frame__fn> reflex_world_frame__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_frame__fn>("reflex_world_frame");
        public static int reflex_world_frame(IntPtr world, float dt, byte[] keys, ReflexPad[] pads, uint padMask) => reflex_world_frame__p.Get()(world, dt, keys, pads, padMask);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_bike__fn(IntPtr world, ref ReflexBikeState state);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike__fn> reflex_world_bike__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike__fn>("reflex_world_bike");
        public static int reflex_world_bike(IntPtr world, ref ReflexBikeState state) => reflex_world_bike__p.Get()(world, ref state);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_rider_pose__fn(IntPtr world, float[] output, uint maxBones);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rider_pose__fn> reflex_world_rider_pose__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rider_pose__fn>("reflex_world_rider_pose");
        public static int reflex_world_rider_pose(IntPtr world, float[] output, uint maxBones) => reflex_world_rider_pose__p.Get()(world, output, maxBones);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_terrain__fn(IntPtr world, ref ReflexTerrainInfo info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain__fn> reflex_world_terrain__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain__fn>("reflex_world_terrain");
        public static int reflex_world_terrain(IntPtr world, ref ReflexTerrainInfo info) => reflex_world_terrain__p.Get()(world, ref info);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_terrain_heights__fn(IntPtr world, float[] output, int x0, int z0, int w, int h);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain_heights__fn> reflex_world_terrain_heights__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain_heights__fn>("reflex_world_terrain_heights");
        public static int reflex_world_terrain_heights(IntPtr world, float[] output, int x0, int z0, int w, int h) => reflex_world_terrain_heights__p.Get()(world, output, x0, z0, w, h);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_set_ambient_animation__fn(IntPtr world, int simulate);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_set_ambient_animation__fn> reflex_world_set_ambient_animation__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_set_ambient_animation__fn>("reflex_world_set_ambient_animation");
        public static int reflex_world_set_ambient_animation(IntPtr world, int simulate) => reflex_world_set_ambient_animation__p.Get()(world, simulate);
        // The native game (native/game): installs the selected ports (mode 0 off, 1 native, 2 verify against the original); the report
        // has one line per installed port (calls, verified, mismatches, first difference).
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_port__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string filter, int mode);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_port__fn> reflex_world_port__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_port__fn>("reflex_world_port");
        public static int reflex_world_port(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string filter, int mode) => reflex_world_port__p.Get()(world, filter, mode);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_port_report__fn(byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_port_report__fn> reflex_world_port_report__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_port_report__fn>("reflex_world_port_report");
        static int reflex_world_port_report(byte[] output, int size) => reflex_world_port_report__p.Get()(output, size);
        /// The ports' report ("" with an older plugin).
        public static string PortReport()
        {
            try
            {
                int n = reflex_world_port_report(null, 0);
                if (n <= 0) return "";
                var b = new byte[n + 1]; reflex_world_port_report(b, b.Length);
                return System.Text.Encoding.UTF8.GetString(b, 0, n);
            }
            catch (EntryPointNotFoundException) { return ""; }
        }
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_terrain_deform__fn(IntPtr world, [Out] float[] output, int x0, int z0, int w, int h);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain_deform__fn> reflex_world_terrain_deform__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_terrain_deform__fn>("reflex_world_terrain_deform");
        public static int reflex_world_terrain_deform(IntPtr world, [Out] float[] output, int x0, int z0, int w, int h) => reflex_world_terrain_deform__p.Get()(world, output, x0, z0, w, h);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_tracks__fn(IntPtr world, [Out] float[] output, uint maxTriangles, out uint version);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_tracks__fn> reflex_world_tracks__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_tracks__fn>("reflex_world_tracks");
        public static int reflex_world_tracks(IntPtr world, [Out] float[] output, uint maxTriangles, out uint version) => reflex_world_tracks__p.Get()(world, output, maxTriangles, out version);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read_floats__fn(IntPtr world, uint address, [Out] float[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read_floats__fn> reflex_world_read_floats__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read_floats__fn>("reflex_world_read");
        public static int reflex_world_read_floats(IntPtr world, uint address, [Out] float[] output, uint size) => reflex_world_read_floats__p.Get()(world, address, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_audio_enable__fn(IntPtr world, int enabled);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_enable__fn> reflex_world_audio_enable__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_enable__fn>("reflex_world_audio_enable");
        public static int reflex_world_audio_enable(IntPtr world, int enabled) => reflex_world_audio_enable__p.Get()(world, enabled);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_audio__fn(IntPtr world, [Out] ReflexAudioCommand[] output, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio__fn> reflex_world_audio__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio__fn>("reflex_world_audio");
        public static int reflex_world_audio(IntPtr world, [Out] ReflexAudioCommand[] output, uint max) => reflex_world_audio__p.Get()(world, output, max);
        /// reflex_world_audio into max * ReflexAudioCommand.NativeSize bytes (pinned, nothing marshalled); ReflexAudioCommand.Decode reads them.
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_audio_raw__fn(IntPtr world, byte[] output, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_raw__fn> reflex_world_audio_raw__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_raw__fn>("reflex_world_audio");
        public static int reflex_world_audio_raw(IntPtr world, byte[] output, uint max) => reflex_world_audio_raw__p.Get()(world, output, max);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_audio_finished__fn(IntPtr world, uint wrapper, uint serial);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_finished__fn> reflex_world_audio_finished__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_audio_finished__fn>("reflex_world_audio_finished");
        public static int reflex_world_audio_finished(IntPtr world, uint wrapper, uint serial) => reflex_world_audio_finished__p.Get()(world, wrapper, serial);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate uint reflex_world_find_object__fn(IntPtr world, uint vtable, uint offset, uint value, uint lo, uint hi);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_find_object__fn> reflex_world_find_object__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_find_object__fn>("reflex_world_find_object");
        public static uint reflex_world_find_object(IntPtr world, uint vtable, uint offset, uint value, uint lo, uint hi) => reflex_world_find_object__p.Get()(world, vtable, offset, value, lo, hi);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_hud__fn(IntPtr world, ref ReflexHudState hud);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_hud__fn> reflex_world_hud__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_hud__fn>("reflex_world_hud");
        public static int reflex_world_hud(IntPtr world, ref ReflexHudState hud) => reflex_world_hud__p.Get()(world, ref hud);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_race_line__fn(IntPtr world, [Out] float[] output, uint maxPoints);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race_line__fn> reflex_world_race_line__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race_line__fn>("reflex_world_race_line");
        public static int reflex_world_race_line(IntPtr world, [Out] float[] output, uint maxPoints) => reflex_world_race_line__p.Get()(world, output, maxPoints);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_camera__fn(IntPtr world, ref ReflexCamera camera);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_camera__fn> reflex_world_camera__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_camera__fn>("reflex_world_camera");
        public static int reflex_world_camera(IntPtr world, ref ReflexCamera camera) => reflex_world_camera__p.Get()(world, ref camera);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_bike_pose__fn(IntPtr world, [Out] float[] output, uint maxNodes);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike_pose__fn> reflex_world_bike_pose__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike_pose__fn>("reflex_world_bike_pose");
        public static int reflex_world_bike_pose(IntPtr world, [Out] float[] output, uint maxNodes) => reflex_world_bike_pose__p.Get()(world, output, maxNodes);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_race__fn(IntPtr world, ref ReflexRaceState race);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race__fn> reflex_world_race__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race__fn>("reflex_world_race");
        public static int reflex_world_race(IntPtr world, ref ReflexRaceState race) => reflex_world_race__p.Get()(world, ref race);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_race_drivers__fn(IntPtr world, [Out] ReflexDriverResult[] output, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race_drivers__fn> reflex_world_race_drivers__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_race_drivers__fn>("reflex_world_race_drivers");
        public static int reflex_world_race_drivers(IntPtr world, [Out] ReflexDriverResult[] output, uint max) => reflex_world_race_drivers__p.Get()(world, output, max);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate uint reflex_world_sim__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_sim__fn> reflex_world_sim__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_sim__fn>("reflex_world_sim");
        public static uint reflex_world_sim(IntPtr world) => reflex_world_sim__p.Get()(world);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read__fn(IntPtr world, uint address, byte[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn> reflex_world_read__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn>("reflex_world_read");
        public static int reflex_world_read(IntPtr world, uint address, byte[] output, uint size) => reflex_world_read__p.Get()(world, address, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate void reflex_world_destroy__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_destroy__fn> reflex_world_destroy__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_destroy__fn>("reflex_world_destroy");
        public static void reflex_world_destroy(IntPtr world) => reflex_world_destroy__p.Get()(world);
        // Every rider of the race (local humans and AI) and a slot's bike model nodes / riding rider's 69 bones (as
        // reflex_world_bike_pose / reflex_world_rider_pose; each node is completed by the game's evaluator 007db890).
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_slots__fn(IntPtr world, [Out] ReflexSlot[] output, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_slots__fn> reflex_world_slots__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_slots__fn>("reflex_world_slots");
        public static int reflex_world_slots(IntPtr world, [Out] ReflexSlot[] output, uint max) => reflex_world_slots__p.Get()(world, output, max);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_bike_pose_slot__fn(IntPtr world, int slot, [Out] float[] output, uint maxNodes);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike_pose_slot__fn> reflex_world_bike_pose_slot__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_bike_pose_slot__fn>("reflex_world_bike_pose_slot");
        public static int reflex_world_bike_pose_slot(IntPtr world, int slot, [Out] float[] output, uint maxNodes) => reflex_world_bike_pose_slot__p.Get()(world, slot, output, maxNodes);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_rider_pose_slot__fn(IntPtr world, int slot, [Out] float[] output, uint maxBones);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rider_pose_slot__fn> reflex_world_rider_pose_slot__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rider_pose_slot__fn>("reflex_world_rider_pose_slot");
        public static int reflex_world_rider_pose_slot(IntPtr world, int slot, [Out] float[] output, uint maxBones) => reflex_world_rider_pose_slot__p.Get()(world, slot, output, maxBones);
        // The game's run-time tables ("Player List", "Rider List", "Vehicle List" and every table of the registry 0xae0910 /
        // the profile databases: PlayerVehicle, PlayerVehicleParts, PlayerRiderGear, AIVehicle, VehiclePart ...).
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate uint reflex_world_table__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table__fn> reflex_world_table__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table__fn>("reflex_world_table");
        public static uint reflex_world_table(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string name) => reflex_world_table__p.Get()(world, name);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_table_rows__fn(IntPtr world, uint table);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_rows__fn> reflex_world_table_rows__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_rows__fn>("reflex_world_table_rows");
        public static int reflex_world_table_rows(IntPtr world, uint table) => reflex_world_table_rows__p.Get()(world, table);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_table_int__fn(IntPtr world, uint table, [MarshalAs(UnmanagedType.LPUTF8Str)] string column, int row, out int value);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_int__fn> reflex_world_table_int__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_int__fn>("reflex_world_table_int");
        public static int reflex_world_table_int(IntPtr world, uint table, [MarshalAs(UnmanagedType.LPUTF8Str)] string column, int row, out int value) => reflex_world_table_int__p.Get()(world, table, column, row, out value);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_table_string__fn(IntPtr world, uint table, [MarshalAs(UnmanagedType.LPUTF8Str)] string column, int row, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_string__fn> reflex_world_table_string__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_table_string__fn>("reflex_world_table_string");
        static int reflex_world_table_string(IntPtr world, uint table, [MarshalAs(UnmanagedType.LPUTF8Str)] string column, int row, byte[] output, int size) => reflex_world_table_string__p.Get()(world, table, column, row, output, size);

        /// A string cell (null: no such column / row).
        public static string TableString(IntPtr world, uint table, string column, int row)
        {
            var b = new byte[256];
            if (reflex_world_table_string(world, table, column, row, b, b.Length) != 0) return null;
            int n = Array.IndexOf(b, (byte)0);
            return System.Text.Encoding.UTF8.GetString(b, 0, n < 0 ? b.Length : n);
        }

        public static string Error(IntPtr world, string fallback)
        {
            try { var p = reflex_world_error(world); return p == IntPtr.Zero ? fallback : (Marshal.PtrToStringUTF8(p) ?? fallback); }
            catch { return fallback; }
        }
    }
}

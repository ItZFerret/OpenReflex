using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

namespace Reflex.Port.Content
{
    // The player's MOTOCAREER profile, kept and changed by the unchanged game itself (native/drafts/world_runtime/career.h,
    // _re/notes/career.md). The PC build saves to Steam Cloud: one file, savegame.bxml (BXML: Version 0x12, PlayerDatabaseVersion,
    // PlayerTableVersion and every Player* table of the profile, then the trailer 0x27720006), written and read by the game's own
    // saved-game manager through SteamRemoteStorage. libreflexworld stands in for Steam Cloud with a folder in the per-user cache
    // (ReflexInstall.CacheDir/save/remote, Steam's own layout), and for Steam with an offline user whose persona name is the
    // motocard name (save/steam_persona.txt): the PC sign-in (PCSignInManager -> MXProfile_SignInWithoutStorage), the Title's
    // storage request and the game's load run as on PC. Nothing here interprets the save: a profile session is the game booted to
    // its front end (reflex_profile_open, ~3.5 s with the JIT), and every question the career pages ask (locks, new items, event
    // statuses, the career completion) is answered by the game's own MXTableHelper functions there.
    public static class ReflexProfile
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_profile_open__fn(string gameDir, string crtDir, string saveDir);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_open__fn> reflex_profile_open__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_open__fn>("reflex_profile_open");
        static IntPtr reflex_profile_open(string gameDir, string crtDir, string saveDir) => reflex_profile_open__p.Get()(gameDir, crtDir, saveDir);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_profile_command__fn(IntPtr profile, string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_command__fn> reflex_profile_command__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_command__fn>("reflex_profile_command");
        static int reflex_profile_command(IntPtr profile, string command, byte[] output, int size) => reflex_profile_command__p.Get()(profile, command, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_profile_error__fn(IntPtr profile);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_error__fn> reflex_profile_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_error__fn>("reflex_profile_error");
        static IntPtr reflex_profile_error(IntPtr profile) => reflex_profile_error__p.Get()(profile);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate void reflex_profile_close__fn(IntPtr profile);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_close__fn> reflex_profile_close__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_profile_close__fn>("reflex_profile_close");
        static void reflex_profile_close(IntPtr profile) => reflex_profile_close__p.Get()(profile);

        /// The boot's writable overlay (ReflexWorldFiles builds use the same folder).
        public static string SaveDir => Path.Combine(ReflexInstall.CacheDir, "save");
        /// Steam Cloud's folder for the game (savegame.bxml).
        public static string RemoteDir => Path.Combine(SaveDir, "remote");
        public static string SaveFile => Path.Combine(RemoteDir, "savegame.bxml");
        static string PersonaFile => Path.Combine(SaveDir, "steam_persona.txt");
        public static bool HasSave => File.Exists(SaveFile);
        /// A persona was entered (the Motocard Setup Name Page; the PC build has the Steam user's).
        public static bool HasPersona => File.Exists(PersonaFile);

        /// The offline Steam user's persona name (the profile's display name in the game): the motocard name.
        public static string Persona
        {
            get { try { var s = File.Exists(PersonaFile) ? File.ReadAllText(PersonaFile).Trim() : ""; return s.Length > 0 ? s : "Player"; } catch (Exception) { return "Player"; } }
            set
            {
                try { Directory.CreateDirectory(SaveDir); File.WriteAllText(PersonaFile, (value ?? "").Trim()); }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { Debug.LogWarning("[Profile] the persona name was not saved: " + e.Message); }   // (a read-only save folder: not an exception in the page that typed it)
            }
        }

        /// A short digest of the save (a career world is made from one save; "none" before the first save).
        public static string SaveStamp()
        {
            try
            {
                if (!HasSave) return "none";
                using var sha = System.Security.Cryptography.SHA1.Create();
                var h = sha.ComputeHash(File.ReadAllBytes(SaveFile));
                var sb = new StringBuilder(); for (int i = 0; i < 6; i++) sb.Append(h[i].ToString("x2")); return sb.ToString();
            }
            catch (Exception) { return "none"; }
        }

        /// No profile session at all (tests of the arcade path: the guest path of a PC without Steam, the profile reminder).
        public static bool Disabled;
        static IntPtr session;
        static Task<bool> opening;
        static readonly object gate = new object();
        /// The profile as the game answered it last (null before the first session).
        public static ProfileData Current { get; private set; }
        public static string Error { get; private set; }
        /// Bumped whenever Current is replaced (pages rebuild).
        public static int Version { get; private set; }
        public static bool IsOpen => Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? CoreGarageProfile.IsOpen : session != IntPtr.Zero;
        public static bool Busy => opening != null && !opening.IsCompleted || pending != null && !pending.IsCompleted;
        static Task<bool> pending;

        /// Boots the game and signs the profile in (background; joins a running open). Current is set when it completes.
        public static Task<bool> Open()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                if (Disabled) return Task.FromResult(false);
                bool ok = CoreGarageProfile.Open();
                if (ok) { Current = CoreGarageProfile.Capture(); Version++; Error = null; } else Error = CoreGarageProfile.Error;
                return Task.FromResult(ok);
            }
            if (Disabled) return Task.FromResult(false);
            lock (gate)
            {
                if (session != IntPtr.Zero) return Task.FromResult(true);
                if (opening != null && !opening.IsCompleted) return opening;
                string game = ReflexInstall.Require(), crt = ReflexCrt.Dir, save = SaveDir;
                Directory.CreateDirectory(save);
                ReflexNativeBoot.Apply();   // the native game in the session's boot (reflex_world_boot_ports)
                opening = Task.Run(() =>
                {
                    IntPtr p;
                    try { p = reflex_profile_open(game, crt, save); }
                    catch (Exception e) { Error = "the profile session is not available (" + e.GetType().Name + ": " + e.Message + ")"; Debug.LogWarning("[Career] " + Error); return false; }   // an older libreflexworld
                    if (p == IntPtr.Zero) { Error = Marshal.PtrToStringAnsi(reflex_profile_error(IntPtr.Zero)) ?? "the profile session did not start"; Debug.LogWarning("[Career] " + Error); return false; }
                    session = p;
                    return Refresh();
                });
                return opening;
            }
        }

        /// Runs front-end operations on the game (career.cpp career::command), then reads the profile again. Background.
        public static Task<bool> Run(params string[] commands)
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                if (Disabled) return Task.FromResult(false);
                bool ok = true; foreach (var command in commands) ok &= Command(command) != null;
                return Task.FromResult(ok);
            }
            if (Disabled) return Task.FromResult(false);
            var before = opening ?? Open();
            Task<bool> prev = pending ?? Task.FromResult(true);
            pending = Task.Run(async () =>
            {
                await prev.ConfigureAwait(false);
                if (!await before.ConfigureAwait(false) && session == IntPtr.Zero) return false;
                bool ok = true;
                foreach (var c in commands) ok &= Command(c) != null;
                return Refresh() && ok;
            });
            return pending;
        }

        /// The "save" command (App.RequestSaveGame) as the front end's Save Popup sees it: writing now, since when (Clock), and whether the
        /// last one wrote the save (the game had a changed table to save: "save requested (queued)").
        public static (bool saving, double started, bool written) SaveState => (saving, saveStarted, saveWritten);
        static volatile bool saving, saveWritten; static double saveStarted;
        static readonly System.Diagnostics.Stopwatch clock = System.Diagnostics.Stopwatch.StartNew();
        /// Seconds of a monotonic clock usable from any thread.
        public static double Clock => clock.Elapsed.TotalSeconds;

        /// One command, blocking (call from a worker thread); null on failure (Error).
        public static string Command(string command)
        {
            if (command == "save")
            {
                saveStarted = Clock; saving = true;
                try { var r = CommandCore(command); saveWritten = r != null && r.Contains("(queued)"); return r; }
                finally { saving = false; }
            }
            return CommandCore(command);
        }
        static string CommandCore(string command)
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                bool ok = CoreGarageProfile.Command(command);
                if (ok) { Current = CoreGarageProfile.Capture(); Version++; Error = null; } else Error = CoreGarageProfile.Error;
                return ok ? "ok (queued)" : null;
            }
            if (session == IntPtr.Zero) { Error = "no profile session"; return null; }
            var buf = new byte[1 << 16];
            int n;
            lock (gate)
            {
                n = reflex_profile_command(session, command, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_profile_command(session, command, buf, buf.Length); }
                if (n < 0)
                {
                    Error = Marshal.PtrToStringAnsi(reflex_profile_error(session)) ?? "command failed: " + command; Debug.LogWarning("[Career] " + command + ": " + Error);
                    if (Error.StartsWith("the profile session stopped", StringComparison.Ordinal)) { var dead = session; session = IntPtr.Zero; opening = null; try { reflex_profile_close(dead); } catch (Exception) { } }   // the next Open boots again
                    return null;
                }
            }
            return Encoding.UTF8.GetString(buf, 0, n);
        }

        static bool Refresh()
        {
            string json = Command("json");
            if (json == null) return false;
            try { Current = ProfileData.Parse(json); Version++; return true; }
            catch (Exception e) { Error = "profile not read: " + e.Message; Debug.LogWarning("[Career] " + Error); return false; }
        }

        /// Ends the session (the game's process; the save stays).
        public static void Close()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) { CoreGarageProfile.Close(); Current = null; return; }
            lock (gate)
            {
                if (session == IntPtr.Zero) return;
                var p = session; session = IntPtr.Zero; opening = null; pending = null;
                try { reflex_profile_close(p); } catch (Exception) { }
            }
        }

        /// An immutable managed snapshot for core session construction.
        public static ProfileData Capture() => CoreGarageProfile.Capture();
        public static Task<bool> SelectRider(int riderId, int gearSet, int vehicleId) => CompleteCoreChange(CoreGarageProfile.SelectRider(riderId, gearSet, vehicleId));
        public static Task<bool> SetTuning(int vehicleId, ReflexGarage.Tuning tuning)
        {
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return Run("lua " + ReflexGarage.EndLua(tuning), "save");
            return CompleteCoreChange(CoreGarageProfile.Tune(vehicleId, tuning));
        }
        public static Task<bool> EquipGear(int gearId, string legacyLua)
        {
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return Run("lua " + legacyLua);
            return CompleteCoreChange(CoreGarageProfile.EquipGear(gearId));
        }
        public static Task<bool> EquipPart(int vehicleId, int partId, string tuning, int one, int two, string legacyLua)
        {
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return Run("lua " + legacyLua);
            return CompleteCoreChange(CoreGarageProfile.EquipPart(vehicleId, partId, tuning, one, two));
        }
        static Task<bool> CompleteCoreChange(bool ok)
        {
            if (ok) { Current = CoreGarageProfile.Capture(); Version++; Error = null; } else { Error = CoreGarageProfile.Error; Debug.LogWarning("[Garage] " + Error); }
            return Task.FromResult(ok);
        }

        /// A new profile read from the save at the next Open (after a race wrote it from the race's world).
        public static void Invalidate() { Close(); }
    }

    /// The profile as the game reported it (career.cpp profile_json).
    public sealed class ProfileData
    {
        public bool signedIn, storage, gameLoaded;
        public float careerCompletion;
        public readonly Dictionary<string, Table> tables = new Dictionary<string, Table>();
        public readonly Dictionary<int, (bool locked, bool hidden, bool isNew, bool dlc)> products = new Dictionary<int, (bool, bool, bool, bool)>();
        public readonly Dictionary<string, int> events = new Dictionary<string, int>(), uiSettings = new Dictionary<string, int>();

        public sealed class Table
        {
            public int rows; public readonly Dictionary<string, List<object>> columns = new Dictionary<string, List<object>>();
            public int Int(string c, int row, int def = -1) => columns.TryGetValue(c, out var l) && row >= 0 && row < l.Count && l[row] is double d ? (int)d : def;
            public float Float(string c, int row) => columns.TryGetValue(c, out var l) && row >= 0 && row < l.Count && l[row] is double d ? (float)d : 0f;
            public string Str(string c, int row) => columns.TryGetValue(c, out var l) && row >= 0 && row < l.Count ? l[row] as string : null;
            /// searchUnique_int: the first row whose column holds v (-1 none).
            public int Find(string c, int v) { if (!columns.TryGetValue(c, out var l)) return -1; for (int i = 0; i < l.Count; i++) if (l[i] is double d && (int)d == v) return i; return -1; }
        }

        public Table T(string name) => tables.TryGetValue(name, out var t) ? t : new Table();
        /// MXTableHelper:IsLocked / IsHidden / IsStoreProductNew / IsStoreProductDLC as the game answered them.
        public bool IsLocked(int sp) => !products.TryGetValue(sp, out var p) || p.locked;
        public bool IsHidden(int sp) => products.TryGetValue(sp, out var p) && p.hidden;
        public bool IsNew(int sp) => products.TryGetValue(sp, out var p) && p.isNew;
        public bool IsDlc(int sp) => products.TryGetValue(sp, out var p) && p.dlc;
        public int EventStatus(string name) => events.TryGetValue(name, out var v) ? v : 0;
        public bool MotocardSetup => Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && CoreGarageProfile.IsOpen || T("PlayerRider").Int("MotocardSetup", 0, 0) != 0;

        public static ProfileData Parse(string json)
        {
            var root = Reflex.Port.OriginalWorld.OriginalUi.ParseJson(json) as Dictionary<string, object>;
            if (root == null) throw new FormatException("not an object");
            var d = new ProfileData();
            if (root.TryGetValue("profile", out var po) && po is Dictionary<string, object> p)
            {
                d.signedIn = N(p, "signedIn") != 0; d.storage = N(p, "storage") != 0; d.gameLoaded = N(p, "gameLoaded") != 0;
            }
            if (root.TryGetValue("tables", out var to) && to is Dictionary<string, object> ts)
                foreach (var kv in ts)
                {
                    if (!(kv.Value is Dictionary<string, object> tv)) continue;
                    var t = new Table { rows = (int)N(tv, "rows") };
                    if (tv.TryGetValue("columns", out var co) && co is Dictionary<string, object> cs)
                        foreach (var c in cs) if (c.Value is List<object> l) t.columns[c.Key] = l;
                    d.tables[kv.Key] = t;
                }
            if (root.TryGetValue("storeProducts", out var so) && so is Dictionary<string, object> sps)
                foreach (var kv in sps)
                    if (kv.Value is List<object> l && l.Count >= 4 && int.TryParse(kv.Key, NumberStyles.Integer, CultureInfo.InvariantCulture, out int id))
                        d.products[id] = (B(l[0]), B(l[1]), B(l[2]), B(l[3]));
            if (root.TryGetValue("events", out var eo) && eo is Dictionary<string, object> es) foreach (var kv in es) d.events[kv.Key] = (int)(kv.Value is double v ? v : 0);
            if (root.TryGetValue("uiSettings", out var uo) && uo is Dictionary<string, object> us) foreach (var kv in us) d.uiSettings[kv.Key] = (int)(kv.Value is double v ? v : 0);
            d.careerCompletion = (float)N(root, "careerCompletion");
            return d;
        }
        static double N(Dictionary<string, object> o, string k) => o.TryGetValue(k, out var v) && v is double d ? d : 0;
        static bool B(object o) => o is double d && d != 0;
    }
}

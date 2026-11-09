using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

namespace Reflex.Port.Content
{
    // What a race leaves in the player's profile, through the unchanged game's own code (native/drafts/world_runtime/persist.h,
    // _re/notes/persistence.md; WP-7 of _re/notes/port-audit.md).
    //   arcade    every non-career world is booted without the save (its image is the same for every player, cached by event). When the
    //             race scene creates it, Attach loads savegame.bxml into the running game as the game loads it (MXProfile_SignIn with
    //             the PC "SaveGame" directory, SavedGameManager_ReadSaveGame, ApplyLoaded) and joins the race's records again (the
    //             activity's PlayerRecords working row, 00764170). The race then counts PlayerRecords (TotalEvents, TotalLaps, TotalTime,
    //             BestLapTime, BestPlace, jumps, holeshots, wheelies / stoppies...) and PlayerHighlight progress exactly as the original.
    //             Leave (the world going away: the post-race CONTINUE, the pause QUIT, the scene unloading) runs what the game runs when
    //             the race is left: PostEventOverlay:OnPostStateComplete's App.RequestSaveGame, the activity teardown's records commit
    //             (PlayerLeft -> 00764560) and the front end's next save, each written by the saved-game worker's write into
    //             ReflexProfile.SaveFile. RESTART (Activity::vf18: the race's records reset to the stored row) commits and saves too, and
    //             the restarted race's world attaches again. Arcade races give no credits in the original (credits come from career
    //             rewards and Free Ride challenges only).
    //   career    career worlds are booted with the save already loaded; AttachCareer only installs the Steam stand-ins (the user's stats)
    //             and ReflexCareer.Finish saves (career::finish_event, which now commits the race's records before CompleteSeries).
    //   steam     ISteamUserStats over <save>/stats/UserGameStats_55140.txt: the 48 achievements (ACHIEVEMENT_CAREER_01 ...
    //             MISC_12, index = HighlightId - 1) set by the game's own unlock (MXHighlightTracking 006d7980 -> 006d87d0) in the
    //             front end, the race and the career completion, stored by MXGame::Update's StoreStats.
    // The front-end pages read them from the profile session: ReflexProfile.Current (tables PlayerHighlight / PlayerRecords, what the
    // game's MotocardHighlights / MotocardRecords Lua read) and ReadAchievements (the profile verb "achievements": per HighlightId the
    // Highlight row, the player's progress and the Steam achievement with its unlock time).
    public static class ReflexPersistence
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_persist__fn(IntPtr world, string saveDir, string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_persist__fn> reflex_world_persist__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_persist__fn>("reflex_world_persist");
        static int reflex_world_persist(IntPtr world, string saveDir, string command, byte[] output, int size) => reflex_world_persist__p.Get()(world, saveDir, command, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_error__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn> reflex_world_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn>("reflex_world_error");
        static IntPtr reflex_world_error(IntPtr world) => reflex_world_error__p.Get()(world);

        static readonly Dictionary<IntPtr, bool> attached = new Dictionary<IntPtr, bool>();   // world -> arcade (the save loaded) / career (Steam only)
        /// False once the plugin answered without reflex_world_persist (an older libreflexworld): nothing is persisted.
        public static bool Available { get; private set; } = true;
        /// The game's report of the last attach / leave (JSON), for tests and logs.
        public static string LastAttach { get; private set; }
        public static string LastLeave { get; private set; }

        /// One persistence command on a world (persist::world_command), between its frames or with them stopped. Null on error (logged).
        public static string Command(IntPtr world, string command)
        {
            if (world == IntPtr.Zero || !Available) return null;
            try
            {
                var buf = new byte[1 << 16];
                int n = reflex_world_persist(world, ReflexProfile.SaveDir, command, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_world_persist(world, ReflexProfile.SaveDir, command, buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Persist] " + command + ": " + Marshal.PtrToStringAnsi(reflex_world_error(world))); return null; }
                return Encoding.UTF8.GetString(buf, 0, n);
            }
            catch (EntryPointNotFoundException) { Available = false; Debug.LogWarning("[Persist] this libreflexworld has no reflex_world_persist: races are not saved"); return null; }
            catch (DllNotFoundException) { Available = false; return null; }
        }

        /// A race world was created (OriginalWorldRuntime.Create, before its first frame): the save into it (arcade), or only the Steam
        /// stand-ins (a career world, which carries its save already).
        public static bool Attach(IntPtr world, bool career)
        {
            if (world == IntPtr.Zero || ReflexProfile.Disabled) return false;
            System.IO.Directory.CreateDirectory(ReflexProfile.SaveDir);
            string r = Command(world, career ? "attach_steam" : "attach");
            if (r == null) return false;
            lock (attached) attached[world] = !career;
            LastAttach = r;
            Debug.Log("[Persist] " + (career ? "career world: the Steam user's stats" : "the profile loaded into the race") + ": " + r);
            return true;
        }

        /// The world is going away (its simulation stopped): the game's saves of a left (restart = false) or restarted race. Null when
        /// the world was not attached; the next profile session reads the new save.
        public static string Leave(IntPtr world, bool restart)
        {
            bool arcade;
            lock (attached) { if (!attached.TryGetValue(world, out arcade)) return null; attached.Remove(world); }
            string r = Command(world, restart ? "restart" : "leave");
            if (r == null) return null;
            if (arcade)
            {
                LastLeave = r;
                ReflexProfile.Invalidate();
                Debug.Log("[Persist] the race " + (restart ? "restarted" : "left") + ": " + (r.Length > 600 ? r.Substring(0, 600) + "..." : r));
            }
            return r;
        }

        // ------------------------------------------------------------------------------------------- achievements (profile session)
        public sealed class Achievement
        {
            public int id, goal, progress, unlockedOrder;   // Highlight.HighlightId / Goal, PlayerHighlight.GoalProgress / UnlockedOrder (-1 none)
            public bool unlocked;                           // progress >= goal (the highlight earned in the profile)
            public string enumName, steamName;              // Highlight.Enum ("Career_01"), the Steam API name ("ACHIEVEMENT_CAREER_01")
            public bool achieved; public uint unlockTime;   // the Steam achievement (unix time)
            public string TitleKey => "STR_HIGHLIGHT_" + id + "_TITLE";               // MXRaven_Highlights (MotocardHighlights.lua)
            public string DescriptionKey => "STR_HIGHLIGHT_" + id + "_DESCRIPTION";
        }
        public sealed class AchievementData
        {
            public readonly List<Achievement> items = new List<Achievement>();
            public int unlocked, achieved;
            public readonly Dictionary<string, double> stats = new Dictionary<string, double>();
            public string file;
            public Achievement ById(int id) => items.Find(a => a.id == id);
        }
        /// The last answer of ReadAchievements (null before).
        public static AchievementData Achievements { get; private set; }

        /// The profile verb "achievements" (blocking: from a worker thread, after ReflexProfile.Open). Null on error.
        public static AchievementData ReadAchievements()
        {
            string json = ReflexProfile.Command("achievements");
            if (json == null) return null;
            try { var d = ParseAchievements(json); Achievements = d; return d; }
            catch (Exception e) { Debug.LogWarning("[Persist] achievements not read: " + e.Message); return null; }
        }
        /// ReadAchievements after the profile session opened (background).
        public static Task<AchievementData> LoadAchievements() => Task.Run(async () => await ReflexProfile.Open().ConfigureAwait(false) ? ReadAchievements() : null);

        public static AchievementData ParseAchievements(string json)
        {
            var o = Reflex.Port.OriginalWorld.OriginalUi.ParseJson(json) as Dictionary<string, object>;
            if (o == null) throw new FormatException("not an object");
            var d = new AchievementData { file = Reflex.Port.OriginalWorld.OriginalUi.S(o, "file") };
            var list = Reflex.Port.OriginalWorld.OriginalUi.Arr(o, "achievements");
            if (list != null)
                foreach (var e in list)
                {
                    float F(string k, float def = 0) => Reflex.Port.OriginalWorld.OriginalUi.F(e, k, def);
                    d.items.Add(new Achievement {
                        id = (int)F("id"), goal = (int)F("goal", -1), progress = (int)F("progress"), unlockedOrder = (int)F("unlockedOrder", -1),
                        unlocked = F("unlocked") != 0, enumName = Reflex.Port.OriginalWorld.OriginalUi.S(e, "enum"), steamName = Reflex.Port.OriginalWorld.OriginalUi.S(e, "name"),
                        achieved = F("achieved") != 0, unlockTime = (uint)F("unlockTime") });
                }
            d.unlocked = (int)Reflex.Port.OriginalWorld.OriginalUi.F(o, "unlocked"); d.achieved = (int)Reflex.Port.OriginalWorld.OriginalUi.F(o, "achieved");
            var st = Reflex.Port.OriginalWorld.OriginalUi.Obj(o, "stats");
            if (st != null) foreach (var kv in st) if (kv.Value is double v) d.stats[kv.Key] = v;
            return d;
        }
    }
}

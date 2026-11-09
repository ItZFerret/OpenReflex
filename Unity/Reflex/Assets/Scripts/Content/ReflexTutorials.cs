using System;

namespace Reflex.Port.Content
{
    // The MotoSkills tutorials (WP-T, _re/notes/tutorials.md; native tutorial.{h,cpp}). The original rides them in the front end's own 3D
    // world (the UI world: MXUIWorld over UI_Compound with its "UI_CompoundFreeRide" section), not in a level of their own: the main menu's
    // MOTOSKILLS (MainMenu:OnTutorialKeypress) equips an MX Lite, App.EnableTutorialMode( TRUE ), "GotoLoadingPage" -> TransitionPageGraph
    // "Tutorial Load Page" (MXUIPlayersLoadingPage: Game_SetupActivityArea, a Free Ride activity in the UI world) -> InGamePageGraph
    // "Freeride Single Player Page" + PendingHUBPopup "ShowTutorialIntroPopup". The game's TutorialManager and page Lua run the six lessons
    // from there. Here that is one event, "Tutorial/UI_Compound": its world is booted through that very flow (ReflexEventSpec.type
    // "Tutorial": the front end's Main Menu Page, MainMenu:OnTutorialKeypress in the game's Lua) and written on the Freeride page; the same
    // image serves every player (the save is loaded into it when the scene creates it, ReflexPersistence.Attach).
    public static class ReflexTutorials
    {
        public const string Key = "Tutorial/UI_Compound";
        public const string Level = "UI_Compound";   // MXRegistry.xml <UISceneName> (MXGame::Startup 00756e10)
        public const int Lessons = 6;                // TutorialPauseMenu.lua m_MaxTutorials; the lessons' Tut<n> scene objects
        /// TutorialPauseMenu.lyr's rows (STR_TUTORIAL_<n>_TITLE) as the game shows them, and the videos PreTutorialPopup plays.
        public static string TitleKey(int lesson) => "STR_TUTORIAL_" + lesson + "_TITLE";
        public static string Video(int lesson) => "Videos\\Tutorial" + lesson;   // PreTutorialPopup:Start ChangeVideoPath( "Videos\\Tutorial" .. n )

        static ReflexEvents.Event _event;
        static readonly object gate = new object();
        /// The tutorial event (null without UI_Compound in the install).
        public static ReflexEvents.Event Event
        {
            get
            {
                lock (gate)
                {
                    if (_event != null) return _event;
                    if (!ReflexInstall.Packages.Has(Level)) return null;
                    int freeRide = ReflexEvents.ActivityIndex("FreeRide");   // the activity Game_SetupActivityArea makes in the UI world (8)
                    return _event = new ReflexEvents.Event
                    {
                        key = Key, typeName = "MotoSkills", location = Level, locationLocId = -1,
                        sceneSection = ReflexEvents.SceneSection(freeRide, Level, 0),   // "UI_CompoundFreeRide" (the lessons' triggers and objects)
                        spec = new ReflexWorldFiles.EventSpec
                        {
                            // tutorial.cpp: the boot takes its own path for this type (EnsureLocalPlayers with the Free Ride activity)
                            type = "Tutorial", location = Level, durationType = "", typeId = -1, locationId = -1, trackId = 0, vehicleClass = -1,
                            riders = 1, ai = 0, duration = 0, activity = freeRide,
                        },
                    };
                }
            }
        }
        public static ReflexEvents.Event Find(string key) => string.Equals(key, Key, StringComparison.OrdinalIgnoreCase) ? Event : Open.Find(key);

        // ------------------------------------------------------------------------------------------------ the open core (w-tutorials)
        /// The MotoSkills tutorials on the OPEN REFLEX core: the race scene runs the core's own tutorial mode in the session's UI world
        /// (rx_session_tutorial_start: no level is loaded, the TutorialManager of the MXUIWorld runs the lessons; core/include/rx_tutorial.h).
        /// The event is the UI world's Free Ride (MXActivityFreeRide in UI_Compound, as the Tutorial Load Page's Game_SetupActivityArea makes it).
        public static class Open
        {
            public sealed class Run { public string stamp; public ReflexEvents.Event ev; public int lesson; }
            public static Run Current;
            public static bool Active => Current != null;
            public const string KeyPrefix = "MotoSkills/";
            /// MainMenu:OnTutorialKeypress (and TutorialPauseMenu:OnAccept of `lesson`).
            public static Run Begin(int lesson)
            {
                string stamp = ReflexProfile.SaveStamp();
                var spec = new ReflexWorldFiles.EventSpec
                {
                    type = "FreeRide", location = Level, durationType = "", typeId = 9, locationId = -1, trackId = 0, vehicleClass = -1, riders = 1, ai = 0, duration = 0, activity = 8,
                    garage = ReflexWorldFiles.GarageSet, riderOwner = -1, vehicleOwner = -1, vehicleType = -1, seriesId = -1, seriesVehicleType = -1, seriesSkipPractice = -1,
                    activation = "", seriesChallenge = -1, exclusiveDevice = -1,
                };
                var ev = new ReflexEvents.Event { key = KeyPrefix + stamp, typeName = "MotoSkills", location = Level, spec = spec, storeProductId = -1, eventTypeLocationId = -1, sceneSection = Level + "FreeRide" };
                Current = new Run { stamp = stamp, ev = ev, lesson = lesson };
                UnityEngine.Debug.Log("[MotoSkills] lesson " + (lesson + 1) + ", save " + stamp);
                return Current;
            }
            public static ReflexEvents.Event Find(string key) => Current != null && string.Equals(key, Current.ev.key, StringComparison.OrdinalIgnoreCase) ? Current.ev : null;
            public static void End() { Current = null; }
        }
        public static bool IsTutorial(ReflexEvents.Event e) => e != null && e.spec.type == "Tutorial";
    }
}

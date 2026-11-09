using System;

namespace Reflex.Port.RxCore
{
    /// MODS/TRACKS (port addition): the custom track the next / current exhibition event rides (Content/TrackMods.cs binds it where the event
    /// scene takes its event, ReflexEventSelect.Awake). OpenReflexSimulation mounts it in the core (rx_session_mount_level: the track's
    /// <level>.dx9.* files read from its folder for that session only) before rx_session_event_load when the spec's location is its level.
    public static class RxCustomTrack
    {
        /// The bound track's level name ("Beta_Track_Slot_1") and folder; null when the event is an original one.
        public static string Level { get; private set; }
        public static string Folder { get; private set; }

        public static void Set(string level, string folder)
        {
            if (string.IsNullOrEmpty(level) || string.IsNullOrEmpty(folder)) { Level = Folder = null; return; }
            Level = level; Folder = folder;
        }
        public static void Reset() => Set(null, null);

        /// Whether the event at `location` rides the bound track.
        public static bool For(string location) => Level != null && string.Equals(location, Level, StringComparison.OrdinalIgnoreCase);
    }
}

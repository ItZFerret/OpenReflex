namespace Reflex.Port.RxCore
{
    /// PRACTICE (port addition): whether the next / current exhibition world is a practice session (an endless
    /// lap race, one rider, no AI; rx_session_set_practice). The PLAY menu's PRACTICE sets it through ReflexEventSelect.Launch (and the
    /// front end's pre-started session, OpenEventPrestart) before the event's session is made; OpenReflexSimulation hands it to the core
    /// before the event loads, as RxMapScale. A RESTART makes a new session with the same flag; the front end clears it (OpenShell.Awake).
    /// Main thread writes; the simulation reads it at construction.
    public static class RxPracticeMode
    {
        /// The world about to load / loaded is a practice session.
        public static bool Current { get; private set; }
        public static void Set(bool on) => Current = on;
        public static void Reset() => Current = false;

        /// The event types (rx_event_spec event_type) that are lap races: practice is made only for these (the core's rule too:
        /// core/game/practice.h rx_practice_lap_type).
        public static bool LapRace(string eventType) =>
            eventType == "National" || eventType == "Supercross" || eventType == "Terracross" || eventType == "Powercross" || eventType == "UTVMoto";
    }
}

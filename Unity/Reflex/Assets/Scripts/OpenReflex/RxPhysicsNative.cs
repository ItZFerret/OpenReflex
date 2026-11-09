using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    /// PHYSICS STUDIO (port addition; core/include/rx_api.h "PHYSICS STUDIO", core/plan/PHYSICS_STUDIO.md): the core's catalog of every
    /// registered vehicle parameter and the terrain grip table, and the session's config. Text in and out (the formats are in rx_api.h).
    /// Every call throws EntryPointNotFoundException on a core without the studio; callers catch it.
    /// ALIVE CLASSES (core/plan/ALIVE_BIKES.md): the models the player rides as a bike with physics of its own (an imported Alive 50cc / 650),
    /// each with its class id, its parameter file and its own hierarchy text (null: none); the game's BikeMods.OwnPhysics sets the provider.
    public static class RxOwnPhysics
    {
        public static Func<System.Collections.Generic.List<(int model, int cls, byte[] file, string name, string nodes)>> Provider;
        public static System.Collections.Generic.List<(int model, int cls, byte[] file, string name, string nodes)> Current()
            => Provider?.Invoke() ?? new System.Collections.Generic.List<(int, int, byte[], string, string)>();
    }

    public static class RxPhysicsNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CatalogFn([MarshalAs(UnmanagedType.LPUTF8Str)] string root, [Out] byte[] output, uint capacity, out uint required);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetFn(IntPtr s, [In] byte[] text, [Out] byte[] report, uint capacity);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TextFn(IntPtr s, [Out] byte[] output, uint capacity, out uint required);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SessionIntFn(IntPtr s, int value);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SessionFn(IntPtr s);
        static readonly RxNativeLoader.Fn<CatalogFn> catalog = new RxNativeLoader.Fn<CatalogFn>("rx_phys_catalog");
        static readonly RxNativeLoader.Fn<SetFn> set = new RxNativeLoader.Fn<SetFn>("rx_session_phys_set");
        static readonly RxNativeLoader.Fn<TextFn> get = new RxNativeLoader.Fn<TextFn>("rx_session_phys_get");
        static readonly RxNativeLoader.Fn<TextFn> live = new RxNativeLoader.Fn<TextFn>("rx_session_phys_live_values");
        static readonly RxNativeLoader.Fn<SessionIntFn> arm = new RxNativeLoader.Fn<SessionIntFn>("rx_session_phys_arm_live");
        static readonly RxNativeLoader.Fn<SessionFn> applyLive = new RxNativeLoader.Fn<SessionFn>("rx_session_phys_apply_live");
        static readonly RxNativeLoader.Fn<SessionFn> active = new RxNativeLoader.Fn<SessionFn>("rx_session_phys_active");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VehiclePhysicsFn(IntPtr s, int model, int cls, [In] byte[] data, uint size, [Out] byte[] report, uint capacity);
        static readonly RxNativeLoader.Fn<VehiclePhysicsFn> vehiclePhysics = new RxNativeLoader.Fn<VehiclePhysicsFn>("rx_session_vehicle_physics");

        /// ALIVE CLASSES (rx_session_vehicle_physics, core/plan/ALIVE_BIKES.md): a parameter file of the mx*_vehicle schema over the class file of
        /// the local players' vehicles of a model (before the event loads); null clears. Returns the records read, negative on an error.
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VehicleNodesFn(IntPtr s, int model, [In] byte[] text);
        static readonly RxNativeLoader.Fn<VehicleNodesFn> vehicleNodes = new RxNativeLoader.Fn<VehicleNodesFn>("rx_session_vehicle_physics_nodes");
        /// ... and the bike's own hierarchy rest locals ("<node> <16 floats>" lines; after VehiclePhysics for the model). Nodes read, < 0 error.
        public static int VehiclePhysicsNodes(IntPtr session, int model, string text) => vehicleNodes.Get()(session, model, Encoding.ASCII.GetBytes((text ?? "") + "\0"));

        public static int VehiclePhysics(IntPtr session, int model, int cls, byte[] file, out string report)
        {
            var rep = new byte[512];
            int n = vehiclePhysics.Get()(session, model, cls, file, file == null ? 0u : (uint)file.Length, rep, (uint)rep.Length);
            report = Decode(rep);
            return n;
        }

        /// The catalog text (H / C / P / V / G / D lines) read from the install (no session needed), or null.
        public static string Catalog(string installRoot)
        {
            var f = catalog.Get();
            if (f(installRoot, null, 0, out uint need) < -1 || need == 0 || need > 64u << 20) return null;
            var b = new byte[need];
            if (f(installRoot, b, need, out need) != 0) return null;
            return Decode(b);
        }

        /// The session's config for its next event (before rx_session_event_load) or, with ApplyLive after, for the running one.
        /// Returns the values set (0: the original game), negative on an error; report lists refused lines and clamps.
        public static int Set(IntPtr session, string text, out string report)
        {
            var bytes = Encoding.UTF8.GetBytes((text ?? "") + "\0");
            var rep = new byte[8192];
            int n = set.Get()(session, bytes, rep, (uint)rep.Length);
            report = Decode(rep);
            return n;
        }
        public static string Get(IntPtr session) => Text(get.Get(), session);
        public static string LiveValues(IntPtr session) => Text(live.Get(), session);
        public static int ArmLive(IntPtr session, bool on) => arm.Get()(session, on ? 1 : 0);
        public static int ApplyLive(IntPtr session) => applyLive.Get()(session);
        public static bool Active(IntPtr session) => active.Get()(session) != 0;
        /// 1: the session's config changes something; | 2: the core's guard put a diverged vehicle back on the file's values.
        public static int ActiveBits(IntPtr session) => active.Get()(session);

        static string Text(TextFn f, IntPtr session)
        {
            if (f(session, null, 0, out uint need) < -1 || need == 0 || need > 64u << 20) return "";
            var b = new byte[need];
            return f(session, b, need, out need) == 0 ? Decode(b) : "";
        }
        static string Decode(byte[] b)
        {
            int n = Array.IndexOf(b, (byte)0);
            return Encoding.UTF8.GetString(b, 0, n < 0 ? b.Length : n);
        }
    }

    /// The PHYSICS STUDIO config the next session uses (the front end sets it: OpenFrontEnd/PhysicsStudio.cs), and what the running
    /// session did with it. Main thread writes, the simulation worker reads at its start (as RxMapScale).
    /// Where a session rides (RxPhysicsStudio.AllowedHere).
    public enum PhysicsContext { Race, FreeRide, TestRide, Career, MotoSkills, Challenge }

    public static class RxPhysicsStudio
    {
        // ------------------------------------------------------------------ the gate (w-physstudio2)
        // ONE place decides whether a player's config may touch a session: races, Free Ride and the test ride always; the career,
        // MotoSkills and the challenges only with OPTIONS > OPEN REFLEX > PHYSICS STUDIO EVERYWHERE (Everywhere).
        /// PHYSICS STUDIO EVERYWHERE (the option): the career, MotoSkills and the challenges too.
        public static volatile bool Everywhere;
        public static bool AllowedHere(PhysicsContext c) =>
            c == PhysicsContext.Race || c == PhysicsContext.FreeRide || c == PhysicsContext.TestRide || Everywhere;
        /// The in-race editor (live changes): races and Free Ride only.
        public static bool LiveEditAllowed(PhysicsContext c) => c == PhysicsContext.Race || c == PhysicsContext.FreeRide;
        /// The core text a new session in context c gets ("" = the original game, the core untouched).
        public static string TextFor(PhysicsContext c) => AllowedHere(c) ? PendingText : "";
        /// Progress of this context is kept only without a config (the career, MotoSkills and challenge rewards: a session that rides one
        /// is practice, nothing of it is saved).
        public static bool PracticeOnly(PhysicsContext c) => c == PhysicsContext.Career || c == PhysicsContext.MotoSkills || c == PhysicsContext.Challenge;

        static volatile string s_text = "", s_name = "";
        /// The core text of the player's active config ("" = the original game: the core is not touched at all).
        public static string PendingText => s_text;
        public static string PendingName => s_name;
        public static void SetPending(string name, string text) { s_name = name ?? ""; s_text = text ?? ""; }

        /// The running event's config: set by the simulation when the core took it (values > 0), cleared at the next session's start.
        public static volatile bool EventActive;
        /// The core's guard put a vehicle the config made diverge back on the file's values (the race badge says so).
        public static volatile bool EventRestored;
        public static volatile string EventName = "";
        public static volatile int EventValues;
        public static volatile string EventReport = "";
        /// The running event's context and whether the in-race editor can change it (the core kept the file values: rx_session_phys_arm_live).
        public static volatile int EventContext;
        public static volatile bool EventLiveArmed;
        /// The running session is practice (a config in the career / MotoSkills / a challenge): its progress is not saved.
        public static volatile bool EventPractice;
    }
}

namespace Reflex.Port.RxCore
{
    /// PHYSICS STUDIO practice sessions (RxPhysicsStudio.PracticeOnly): the profile the session starts with is imported as usual, but nothing
    /// it ends or checkpoints with is published, so a career event, a MotoSkills lesson or a challenge ridden with a player's config keeps
    /// no progress, rewards or records. The caller is told it was saved (the career's flow goes on); the profile on disk is as before.
    public class PhysicsPracticePersistence : IPlayerSessionPersistence, IPlayerCheckpointReceiptFeedback
    {
        protected readonly IPlayerSessionPersistence inner;
        readonly Action<string> log;
        protected PhysicsPracticePersistence(IPlayerSessionPersistence inner, Action<string> log) { this.inner = inner; this.log = log; }
        public static IPlayerSessionPersistence Wrap(IPlayerSessionPersistence inner, Action<string> log) =>
            inner == null ? null : inner is IPlayerNonterminalPersistence ? new Nonterminal(inner, log) : new PhysicsPracticePersistence(inner, log);
        public byte[] Prepare(byte[] nativeDefaults) => inner.Prepare(nativeDefaults);
        public void Imported(byte[] validatedSnapshot) => inner.Imported(validatedSnapshot);
        public void Complete(byte[] settledSnapshot) => log?.Invoke("PHYSICS STUDIO practice: the session's progress is not saved (it rode a physics config)");
        public void Failed(string error) => inner.Failed(error);
        public void Receipt(string message) { if (inner is IPlayerCheckpointReceiptFeedback f) f.Receipt(message); else if (message != null) inner.Failed(message); }
        sealed class Nonterminal : PhysicsPracticePersistence, IPlayerNonterminalPersistence
        {
            public Nonterminal(IPlayerSessionPersistence inner, Action<string> log) : base(inner, log) { }
            public void Checkpoint(byte[] validatedSnapshot) { }
        }
    }
}

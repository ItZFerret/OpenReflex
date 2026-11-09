// WP-G: the garage's tuning and test drive through the unchanged game (native/drafts/world_runtime/garage.h, _re/notes/garage.md).
//   tuning      PlayerVehicleTuning (Engine1/2, Suspension1/2, Tire1/2: bonus points over VehicleModel DefaultEngine1/2 ...; Braking /
//               Steering 0 / 50 / 100). GarageVehicleTuning:End writes the row of PlayerRider.LastEquippedVehicle and calls
//               App.ApplyVehicleTuning( device ) (00743550 -> VehicleSpec_ApplyTuning 00543130 -> RavenVehicle_SetTuning 0045f270: the
//               running vehicle's V+0x220.. and its simulation's S+0x2ef0..), then App.RequestSaveGame. The vehicle factory applies the same
//               function when it makes a vehicle, so a world booted from the save (a career or test-drive world) starts with the tuning;
//               an arcade world (booted with the default profile, the save attached at its first frame: ReflexPersistence) gets it from
//               App.ApplyVehicleTuning right after the attach, as the pause popup does.
//   test drive  GarageHUB:OnStartPressed in the front end's own world UI_Compound (MXGarage TestDriveStart): a world booted with the
//               save (reflex_world_build with REFLEX_TESTDRIVE_SET), named by the save's digest; the pause request ends it (the Garage Page).
using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ReflexGarage
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_garage__fn(IntPtr world, string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_garage__fn> reflex_world_garage__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_garage__fn>("reflex_world_garage");
        static int reflex_world_garage(IntPtr world, string command, byte[] output, int size) => reflex_world_garage__p.Get()(world, command, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_error__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn> reflex_world_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn>("reflex_world_error");
        static IntPtr reflex_world_error(IntPtr world) => reflex_world_error__p.Get()(world);
        /// False once the plugin answered that it has no reflex_world_garage (an older libreflexworld: tuning stays the world's own).
        public static bool Available { get; private set; } = true;

        /// garage::world_command on a running world, between its frames: "lua <source>", "apply_tuning <device>", "tuning <slot>". Null on error.
        public static string WorldCommand(IntPtr world, string command)
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex || world == IntPtr.Zero || !Available) return null;
            try
            {
                var buf = new byte[1 << 12];
                int n = reflex_world_garage(world, command, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_world_garage(world, command, buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Garage] " + command.Substring(0, Math.Min(60, command.Length)) + ": " + Marshal.PtrToStringAnsi(reflex_world_error(world))); return null; }
                return Encoding.UTF8.GetString(buf, 0, n);
            }
            catch (EntryPointNotFoundException) { Available = false; Debug.LogWarning("[Garage] this libreflexworld has no reflex_world_garage: the tuning is the world's own"); return null; }
            catch (DllNotFoundException) { Available = false; return null; }
        }

        /// The plugin builds test-drive worlds and runs the garage's world commands (reflex_world_garage; an older libreflexworld has neither).
        public static bool PluginHasTestDrive
        {
            get
            {
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return false;
                if (pluginChecked) return pluginHas;
                pluginChecked = true;
                try { reflex_world_garage(IntPtr.Zero, "", null, 0); pluginHas = true; } catch (EntryPointNotFoundException) { pluginHas = false; } catch (DllNotFoundException) { pluginHas = false; }
                return pluginHas;
            }
        }
        static bool pluginChecked, pluginHas;

        /// The eight PlayerVehicleTuning values GarageVehicleTuning:End writes (the attribute bonuses, then Braking / Steering 0 / 50 / 100).
        [Serializable]
        public struct Tuning
        {
            public int engine1, engine2, suspension1, suspension2, tire1, tire2, braking, steering;
            public override string ToString() => $"engine {engine1}/{engine2} suspension {suspension1}/{suspension2} tires {tire1}/{tire2} braking {braking} steering {steering}";
        }

        /// GarageVehicleTuning:End's table writes and App.ApplyVehicleTuning( device ) as the game's Lua (one line; the profile session's
        /// and a world's "lua"). The row is the one of PlayerRider.LastEquippedVehicle (customTuningTable:searchUnique_int).
        public static string EndLua(Tuning t, int device = 0)
        {
            var c = CultureInfo.InvariantCulture;
            return string.Format(c,
                "local d = {0}; local t = App.LookupPlayerTable( \"PlayerVehicleTuning\", d ); local id = App.LookupPlayerTable( \"PlayerRider\", d ):cell_int( 0, \"LastEquippedVehicle\" ); " +
                "local r = t:searchUnique_int( \"PlayerVehicleId\", id ); " +
                "r:setCell_int( \"Engine1\", {1} ); r:setCell_int( \"Engine2\", {2} ); r:setCell_int( \"Suspension1\", {3} ); r:setCell_int( \"Suspension2\", {4} ); " +
                "r:setCell_int( \"Tire1\", {5} ); r:setCell_int( \"Tire2\", {6} ); r:setCell_int( \"Braking\", {7} ); r:setCell_int( \"Steering\", {8} ); " +
                "App.ApplyVehicleTuning( d )",
                device, t.engine1, t.engine2, t.suspension1, t.suspension2, t.tire1, t.tire2, t.braking, t.steering);
        }

        /// The race scene's running world (set at its creation; the pause page's tuning popup reads it while the simulation is idle).
        public static IntPtr World { get; private set; }

        /// The tuning of a world's slot as the game holds it ("tuning <slot>" JSON: the spec's eight values, V+0x220.., S+0x2ef0..). Null on error.
        public static string State(IntPtr world, int slot = 0) => WorldCommand(world, "tuning " + slot);

        /// An arcade race on the player's own vehicle: the attached save's PlayerVehicleTuning onto the running vehicle, as
        /// App.ApplyVehicleTuning does (OriginalWorldRuntime.Create, after ReflexPersistence.Attach, before the first frame). Default tuning
        /// leaves the bike state unchanged (garage.md: Concord 978d2bfd948de7ce with and without).
        public static void ApplyAttachedTuning(IntPtr world, ReflexWorldFiles.Garage garage)
        {
            World = world;
            if (TestDrive.Active && world != IntPtr.Zero) TestDrive.Watch();   // the game's own pause request ends a test drive
            if (world == IntPtr.Zero || ReflexCareer.Current != null || TestDrive.Active) return;   // booted from the save: made with it
            if (garage != null && garage.vehicleOwner >= 0 && garage.vehicleOwner != ReflexWorldFiles.Garage.Player1) return;   // a pro's vehicle: no tuning row
            if (WorldCommand(world, "apply_tuning 0") != null) Debug.Log("[Garage] the save's vehicle tuning on the race's vehicle: " + State(world));
        }

        // ------------------------------------------------------------------------------------------------ test drive
        public static class TestDrive
        {
            /// The test drive the race scene runs (null: none). Set by the garage's TEST DRIVE, cleared back in the front end.
            public sealed class Run { public string stamp; public ReflexEvents.Event ev; }
            public static Run Current;
            public static bool Active => Current != null;
            public const string KeyPrefix = "TestDrive/";
            public const int TestDriveSet = 0x56495244;   // REFLEX_TESTDRIVE_SET ('DRIV')
            /// The front end's own world (MXRegistry UISceneName), where the test drive runs (MXActivityFreeRide).
            public const string Level = "UI_Compound";

            /// GarageHUB:OnStartPressed: a test drive of the profile's equipped vehicle from the current save.
            public static Run Begin()
            {
                string stamp = ReflexProfile.SaveStamp();
                Current = new Run { stamp = stamp, ev = EventOf(stamp) };
                Debug.Log("[Garage] test drive of the equipped vehicle, save " + stamp);
                return Current;
            }
            /// The test drive's event (FreeRide in the UI world; its key names the save) without starting one (the load arena kept warm in the
            /// menus makes the session the race scene would: OpenFrontEnd/OpenArenaWarm.cs).
            public static ReflexEvents.Event EventOf(string stamp)
            {
                var spec = new ReflexWorldFiles.EventSpec
                {
                    type = "FreeRide", location = Level, durationType = "", typeId = 9, locationId = -1, trackId = 0, vehicleClass = -1, riders = 1, ai = 0, duration = 0, activity = 8,
                    garage = ReflexWorldFiles.GarageSet, riderOwner = -1, vehicleOwner = -1, vehicleType = -1, seriesId = -1, seriesVehicleType = -1, seriesSkipPractice = -1,
                    activation = "", seriesChallenge = -1, exclusiveDevice = -1, testDrive = TestDriveSet,
                };
                return new ReflexEvents.Event { key = KeyPrefix + stamp, typeName = "Free Ride", location = Level, spec = spec, storeProductId = -1, eventTypeLocationId = -1, sceneSection = Level + "FreeRide" };
            }
            /// ReflexEvents.Find's last resort: the running test drive's event.
            public static ReflexEvents.Event Find(string key) => Current != null && string.Equals(key, Current.ev.key, StringComparison.OrdinalIgnoreCase) ? Current.ev : null;
            public static void End() { Current = null; }

            /// The pause request in a test drive (the game's own, its UI live: MXUIManager_OnPauseRequest 00598e40) ends it in the world:
            /// ui+0x290 back to 0 (MXUIManager_EnableTestDrive( FALSE )), the hub route "GotoGarageVehiclePage" (not an App.SendUIEvent: the
            /// hub's GotoHub). The Test Drive Exit Page would then unload the level and load the garage (MXGarage TestDriveEnd), which the world
            /// runtime cannot (no device: it stops ~45 frames later), so the port goes back to its Garage Page as soon as the flag drops.
            public static void Watch()
            {
                var rt = UnityEngine.Object.FindAnyObjectByType<Reflex.Port.OriginalWorld.OriginalWorldRuntime>();
                if (rt && !rt.TryGetComponent<Watcher>(out _)) rt.gameObject.AddComponent<Watcher>().runtime = rt;
            }
            /// Tests: the flag's last reading (-1 not read yet).
            public static int LastFlag = -1;
            sealed class Watcher : MonoBehaviour
            {
                public Reflex.Port.OriginalWorld.OriginalWorldRuntime runtime;
                volatile int flag = -1; bool seen, leaving;
                readonly byte[] buf = new byte[4];
                void Update()
                {
                    if (!runtime || leaving || !Active) return;
                    var sim = runtime.Simulation; if (sim == null) return;
                    sim.BetweenFrames(w =>
                    {
                        uint Rd(uint a) => Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(w, a, buf, 4) == 0 ? BitConverter.ToUInt32(buf, 0) : 0;
                        uint game = Rd(0xd728d8), ui = game != 0 ? Rd(game + 0x745c) : 0;
                        flag = ui != 0 ? (int)Rd(ui + 0x290) : -1;
                    });
                    int f = flag; LastFlag = f;
                    if (f == 1) seen = true;
                    else if (f == 0 && seen)
                    {
                        leaving = true;
                        Debug.Log("[Garage] the test drive ended (the pause request): back to the garage");
                        runtime.StopSimulation();
                        Reflex.Port.OriginalWorld.OriginalPauseMenu.QuitToFrontEnd();
                    }
                }
            }
        }
    }
}

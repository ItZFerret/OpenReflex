using System;
using System.Globalization;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.OriginalWorld
{
    // The pause page's VEHICLE LIST (WP-5; Free Ride): the game's own VehiclesQuickViewPopup runs in the world and is mirrored live
    // (OriginalLiveUi). Its accept ends in PauseMenu:OnQuickView{Player,AI}VehicleChanged, which in the game equips the vehicle and loads
    // it in the same level (SyncDatabaseWithManagers, EnableWorld, the Vehicle Loading Page). The world runtime cannot load (OriginalEventIntro
    // patches those two handlers to leave the choice in UI "ReflexQuickView" and send GotoVehicleLoadingPage, held back by modes.cpp), so
    // the port does what the Vehicle Loading Page amounts to: the same Free Ride with the chosen vehicle, a world booted with that garage
    // choice (MXTableHelper EquipVehicle / EquipRider before SetupEvent, as VehicleSelect:End; ReflexWorldFiles.Garage).
    public static class OriginalQuickView
    {
        /// The choice read from the world ("p|<PlayerVehicleId>" / "a|<AIVehicleId>|<AIRiderId>|<gear set>"); the last one (tests).
        public static string LastChoice;

        /// GotoVehicleLoadingPage from the live UI: reload the event with the vehicle the quick view chose. False: not a quick view choice.
        public static bool Reload(OriginalWorldRuntime runtime, string eventKey)
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim == null || string.IsNullOrEmpty(eventKey)) return false;
            string scene = SceneManager.GetActiveScene().name;
            Runner.Ensure();   // on the main thread
            sim.BetweenFrames(w =>
            {
                string choice = OriginalEventIntro.Command(w, "ui ReflexQuickView");
                Dispatch(() => Launch(runtime, eventKey, scene, choice));
            });
            return true;
        }

        static void Launch(OriginalWorldRuntime runtime, string eventKey, string scene, string choice)
        {
            LastChoice = choice;
            var f = (choice ?? "").Split('|');
            int I(int i) => i < f.Length && int.TryParse(f[i], NumberStyles.Integer, CultureInfo.InvariantCulture, out int v) ? v : -1;
            ReflexWorldFiles.Garage g = null;
            if (f.Length >= 2 && f[0] == "p" && I(1) >= 0)   // the player's own vehicle; EquipRider( row, 0, 0, Player owner )
                g = new ReflexWorldFiles.Garage { vehicleOwner = ReflexWorldFiles.Garage.Player1, vehicleId = I(1), riderOwner = ReflexWorldFiles.Garage.Player1, riderId = 0, riderGearSet = 0 };
            else if (f.Length >= 4 && f[0] == "a" && I(1) >= 0)   // a pro rider's vehicle with that rider and his default gear set
                g = new ReflexWorldFiles.Garage { vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = I(1), riderOwner = I(2) >= 0 ? ReflexWorldFiles.Garage.AI : -1, riderId = Math.Max(0, I(2)), riderGearSet = Math.Max(0, I(3)) };
            Debug.Log("[Pages] VEHICLE LIST: " + (choice ?? "(no choice)") + " -> " + (g != null ? "reload " + eventKey : "the front end"));
            if (runtime) runtime.StopSimulation();
            if (g == null) { OriginalPauseMenu.QuitToFrontEnd(); return; }
            ReflexEventSelect.Launch(eventKey, 0, false, scene, 0, g);
        }

        // back to the main thread (the world's answer comes on the simulation thread)
        static readonly System.Collections.Concurrent.ConcurrentQueue<Action> main = new System.Collections.Concurrent.ConcurrentQueue<Action>();
        static void Dispatch(Action a) => main.Enqueue(a);
        sealed class Runner : MonoBehaviour
        {
            static Runner instance;
            public static void Ensure() { if (instance) return; var go = new GameObject("Reflex quick view") { hideFlags = HideFlags.HideAndDontSave }; instance = go.AddComponent<Runner>(); }
            void Update() { while (main.TryDequeue(out var a)) { try { a(); } catch (Exception e) { Debug.LogWarning("[Pages] " + e.Message); } } }
        }
    }
}

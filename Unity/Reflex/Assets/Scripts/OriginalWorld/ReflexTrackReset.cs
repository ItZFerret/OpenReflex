// RESET TRACK (w-fb-hud; a playtester's request: "quick reset deform in time trial / free ride instead of having to restart the race (single
// player only)"): the ruts, berms and tyre tracks back to the untouched track, without reloading anything and with the rider kept where he is.
//   how     the game's own level reset of the deformable terrain (core rx_session_terrain_reset: the terrain +0x44b8 the activity restart
//           Activity::vf18 arms; the next frame HFTerrainSurface::vf14 clears both deformation targets, reads the cleared grid back into what
//           the physics reads and resets the three tyre-track sets). The terrain mesh / material and the tyre-track ribbons follow through
//           the usual patches (the export resyncs: one whole-grid patch to OriginalTerrain, the tracks' list to OriginalTracks); the replay
//           records that patch as any other. The riders, the activity, its clock and laps, the mud on the riders stay.
//   where   offline only, single rider: Free Ride (not its challenges, the test ride or MotoSkills), PRACTICE, and an arcade race ridden
//           without AI (one rider); never in the career or in a race with AI (Unavailable says why).
//   from    the pause page's RESET TRACK row (OwnRacePause; OwnModes.Pause: spec.resetTrack) and an optional control of its own (CONTROLS >
//           RESET TRACK: a key and a pad input, both unbound by default; ReflexSettings.ResetTrackKey / ResetTrackPad).
//   practice the lap being ridden is not counted (TRACK RESET in the lap list: the track changed under it); the next laps count.
using System;
using Reflex.Port.FrontEnd;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    [DefaultExecutionOrder(51)]
    public sealed class ReflexTrackReset : MonoBehaviour
    {
        public static ReflexTrackReset Instance { get; private set; }
        /// Resets the core armed since the start (tests): the count of 0 answers.
        public static int Count { get; private set; }
        /// The last answer of the core (int.MinValue none yet; 0 armed).
        public static int LastResult { get; private set; } = int.MinValue;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (Instance) return;
            var go = new GameObject("TrackReset") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            Instance = go.AddComponent<ReflexTrackReset>();
        }

        // ------------------------------------------------------------------------------------------------ availability
        public const string OnlyHere = "RESET TRACK IS FOR FREE RIDE, PRACTICE AND RACES WITHOUT AI";
        /// Why RESET TRACK is off in this ride ("" = it works here).
        public static string Unavailable(OriginalWorldRuntime rt)
        {
            if (!rt || !rt.Running) return "NOT NOW";
            if (rt.SimDriver?.Open == null) return "NEEDS THE OPEN REFLEX CORE";
            if (rt.OwnedCareerActive || rt.SimDriver.Open.Career != null) return "NOT IN THE CAREER";
            if (rt.testRide || rt.tutorial || rt.IsChallengeWorld) return OnlyHere;
            string id = rt.EventIdentity ?? "";
            if (id.StartsWith("FreeRide/", StringComparison.Ordinal)) return "";
            if (rt.IsPractice) return "";
            var race = rt.Race;
            if (race.valid != 0 && race.riders == 1 && id.Length > 0) return "";   // an arcade race ridden alone (no AI): a time trial
            return OnlyHere;
        }

        /// Arms the reset (the next frame). false (with a notice): not here / not now.
        public static bool Reset(OriginalWorldRuntime rt)
        {
            string why = Unavailable(rt);
            if (why.Length > 0) { Notice(why, ""); return false; }
            var m = Instance;
            m.pending = int.MinValue; m.pendingWorld = rt.WorldGeneration;
            rt.SimDriver.Open.ResetTrack(rc => m.pending = rc);
            Debug.Log("[TrackReset] RESET TRACK asked (" + rt.EventIdentity + ")");
            return true;
        }

        volatile int pending = int.MinValue; long pendingWorld = -1;
        SpawnMarkerNotice notice;
        bool keyWas, padWas;

        static void Notice(string title, string sub)
        {
            var m = Instance; if (!m || ReflexSettings.HudHidden) return;
            if (!m.notice) m.notice = SpawnMarkerNotice.Create(m.transform);
            m.notice.Show(title, sub);
        }

        void Update()
        {
            var rt = ReflexClientLive.Runtime;
            int rc = pending;
            if (rc != int.MinValue)
            {
                pending = int.MinValue; LastResult = rc;
                if (rc == 0)
                {
                    Count++;
                    bool practice = rt && rt.WorldGeneration == pendingWorld && rt.IsPractice;
                    if (practice) Reflex.Port.Practice.RxPractice.TrackResetUsed();
                    Debug.Log("[TrackReset] the track is reset (the ruts and the tyre tracks)" + (practice ? "; the lap being ridden is not counted" : ""));
                    Notice("TRACK RESET", practice ? "THIS LAP IS NOT COUNTED (TRACK RESET)" : "THE RUTS AND TYRE TRACKS ARE GONE");
                }
                else Notice(rc == 2 ? "NO TRACK TO RESET HERE" : "NEEDS THE CURRENT CORE", "");
            }
            // the optional control (unbound by default): a press while riding, never in a menu
            if (!rt) { keyWas = padWas = false; return; }
            int dik = ReflexSettings.ResetTrackKey, pid = ReflexSettings.ResetTrackPad;
            if (dik == 0 && pid == 0) return;
            var kb = Keyboard.current;
            var k = dik != 0 ? SpawnMarkerInput.KeyOf(dik) : Key.None;
            bool keyDown = kb != null && k != Key.None && !OriginalOnboardAdjust.KeyboardCaptured && kb[k].isPressed;
            bool padDown = pid != 0 && SpawnMarkerInput.PadDown(rt.PadRouting.Pad(0), pid);
            bool pressed = keyDown && !keyWas || padDown && !padWas;
            keyWas = keyDown; padWas = padDown;
            if (!pressed || Blocked(rt)) return;
            Reset(rt);
        }

        static bool Blocked(OriginalWorldRuntime rt) =>
            rt.paused || rt.OwnMenuActive || rt.IntroCardShown || rt.optionsMenu && rt.optionsMenu.IsOpen || rt.pauseMenu && rt.pauseMenu.IsOpen ||
            ReflexOptions.IsOpen || Reflex.Port.OpenFrontEnd.PhysicsStudioOverlay.Shown;
    }
}

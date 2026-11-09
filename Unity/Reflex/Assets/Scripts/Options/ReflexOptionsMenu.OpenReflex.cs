// OPTIONS > OPEN REFLEX: the settings of OPEN REFLEX itself that change how the game plays (ours, not the original game's).
//   RIDING   AUTO LEVEL and COUNTER-STEER SNAP (core rx_session_set_air_helpers, core/notes/bike.md "FREER AIR"): 0 .. 100 %, how much of the original's
//            two in-air helpers that turn the bike back toward its take-off angle the player's bike / ATV keeps (never the computer riders'): 100 % =
//            the original game (the default), 0 % = removed, after Open Unleashed's FREER AIR ROTATION, in between they work that much slower. They
//            replace the earlier FREER AIR toggle (a saved FREER AIR on reads as both at 0 %, its KEEP choices as 100 %). AIR LOOSENESS (w-freeair2,
//            core rx_session_set_free_air_looseness): 0 .. 100 %, the air damping while coasting, the stick lag and authority eased; off at 0 %.
//            All apply at once, in a race too (the next airborne step).
//   FREE RIDE  SPAWN MARKERS (w-spawnmarker, w-markerbtns; OriginalWorld/OriginalSpawnMarker.cs, core rx_session_spawn_at_instant): ON by default; hold Y / F to set, B / V to respawn.
//   FREE RIDE  NO INVISIBLE BORDERS (w-frnowalls, w-reyesborders; core rx_session_set_free_ride_borders, game/free_ride_borders.h): ON by default: no
//            reset zones, no reset triggers but deep water, no cannonball band (the FR maps' white-out boundary), the terrain's edge only as a fall net,
//            in Free Ride only. Collision is never left out (w-dfcollide: the DoverB "panel" w-frnowalls left out was the JumboTron's screen).
//   FREE RIDE  ALIVE LOOK (w-alivereflex experiment; Content/AliveLook.cs): REFLEX / ALIVE, how the MX vs ATV Alive tracks are drawn; read when a track loads.
//   PHYSICS STUDIO   PHYSICS STUDIO EVERYWHERE (w-physstudio2; RxPhysicsStudio.Everywhere): the player's config also in the career,
//            MotoSkills and the challenges (practice there).
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        VisualElement BuildOpenReflex(VisualElement panel)
        {
            Section(panel, "RIDING");
            var first = Named(OptUi.Slider("AUTO LEVEL", () => ReflexSettings.AirAutoLevel, v =>
            {
                ReflexSettings.AirAutoLevel = v; ReflexSettings.Save(); ReflexClientLive.ApplyFreeAir();
            }, panel, 0f, 1f, 0.05f, v => Mathf.RoundToInt(v * 100f) + "%"), "air-auto-level");
            Note(panel, "How much the original's auto-level helps you in the air. Once you have used the sticks in a jump, letting go turns the bike back toward level with its nose along the jump " +
                        "(half way in about 1 to 1.5 s), which also lines it up for the landing. 100% (the default) is the original game. Lower is slower: you keep more of the angle you let go at, " +
                        "and set up the landing yourself. 0% turns it off: the bike stays where you leave it and keeps turning a little on its own momentum, like Open Unleashed's FREER AIR. " +
                        "Your bike or ATV only (the computer riders keep the original's air). Applies at once, also in a race. Ours.");
            Named(OptUi.Slider("COUNTER-STEER SNAP", () => ReflexSettings.AirSnap, v =>
            {
                ReflexSettings.AirSnap = v; ReflexSettings.Save(); ReflexClientLive.ApplyFreeAir();
            }, panel, 0f, 1f, 0.05f, v => Mathf.RoundToInt(v * 100f) + "%"), "air-counter-steer-snap");
            Note(panel, "How hard the original snaps you back out of a whip. After a big whip, pushing the steering stick hard the other way (past 90 %) takes the stick away and snaps the bike back toward its take-off angle. " +
                        "100% (the default) is the original game. Lower snaps back slower. 0% turns it off: that counter-steer is yours, and it turns the bike back at the stick's own rate, slower than the snap. Applies at once. Ours.");
            Named(OptUi.Slider("AIR LOOSENESS", () => ReflexSettings.AirLooseness, v =>
            {
                ReflexSettings.AirLooseness = v; ReflexSettings.Save(); ReflexClientLive.ApplyFreeAir();
            }, panel, 0f, 1f, 0.05f, v => Mathf.RoundToInt(v * 100f) + "%"), "air-looseness");
            Note(panel, "How loose the bike feels in the air: higher lets a whip or spin carry on longer after you let go, answers the stick a little quicker and gives the stick a little more say. " +
                        "0% (the default) is the original's air; it works with any AUTO LEVEL and COUNTER-STEER SNAP, and goes best with them lowered. 100% is the loosest, still smooth. Jumps you don't touch the sticks on stay the same. Applies at once. Ours.");
            Section(panel, "FREE RIDE");
            Named(OptUi.Toggle("NO INVISIBLE BORDERS", () => ReflexSettings.FreeRideNoBorders, v =>
            {
                ReflexSettings.FreeRideNoBorders = v; ReflexSettings.Save(); ReflexClientLive.ApplyFreeRideBorders();
            }, panel), "free-ride-no-borders");
            Note(panel, "ON (the default): in Free Ride you are never put back for where you ride. No reset when you leave the track or ride into an out-of-bounds area, " +
                        "no white-out near the edge of the Free Ride maps, and no reset boxes (tunnels, boats, cliffs). Walls, fences, buildings, screens and the ground stay solid. " +
                        "Kept: your own reset and save spot, the crash reset, deep water, and a safety net at the true edge of the map: ride off the end of the ground and you are " +
                        "brought back after 4 s of falling. OFF: the original's borders. Races, the career, the challenges, MotoSkills and the test ride always keep the original's. Ours.");
            Named(OptUi.Toggle("ALIVE LOOK", () => ReflexSettings.AliveLookReflex, v =>
            {
                ReflexSettings.AliveLookReflex = v; ReflexSettings.Save();
            }, panel, "REFLEX", "ALIVE"), "alive-look");
            Note(panel, "How the MX vs ATV Alive tracks are drawn (an experiment). REFLEX: with this game's own terrain, lighting, shadows and fog: a similar Reflex track's dirt " +
                        "textures, ruts, sun, fill light and haze over the Alive track's layout and objects. ALIVE: as MX vs ATV Alive drew them. From the next track you load. Ours.");
            Named(OptUi.Toggle("SPAWN MARKERS", () => ReflexSettings.SpawnMarkers, v =>
            {
                ReflexSettings.SpawnMarkers = v; ReflexSettings.Save();
            }, panel), "spawn-markers");
            Note(panel, "ON (the default): in Free Ride, hold SET SPAWN MARKER (the pad's Y, or F) for about half a second to keep your place and heading as this track's marker; " +
                        "press RIDE TO SPAWN MARKER (B, or V) to ride back there at once (change them under CONTROLS). The respawn is the game's own reset (upright, speed gone, a crash cleared, " +
                        "the short see-through ghost time after it), only at your marker and without the reset key's white fade and wait. " +
                        "Kept for each track and map scale. Also in PRACTICE, any time: riding back there ends the lap (not counted: MARKER) and the timing starts again at the finish line, as in MX Bikes. " +
                        "Free Ride and PRACTICE only: never in races, challenges, MotoSkills or the test ride. " +
                        "Y / B (F / V) are the game's own reset-point buttons (a flag where you are, back to the flag): where markers work the spawn marker replaces them; in races and the career they stay the game's. " +
                        "OFF: Y / B are the game's flag everywhere. Ours.");
            Section(panel, "PHYSICS STUDIO");
            Named(OptUi.Toggle("PHYSICS STUDIO EVERYWHERE", () => ReflexSettings.PhysicsEverywhere, v =>
            {
                ReflexSettings.PhysicsEverywhere = v; ReflexSettings.Save(); Reflex.Port.RxCore.RxPhysicsStudio.Everywhere = v;
            }, panel), "physics-everywhere");
            Note(panel, "OFF (the default): your PHYSICS STUDIO config rides in races, Free Ride and the test ride only. ON: also in the career, MotoSkills and the challenges. " +
                        "There an event with a config is practice: no progress, rewards or records are saved. From the next event. Ours.");
            BuildReplayOptions(panel);   // REPLAY / PHOTO MODE (ReflexOptionsMenu.Replay.cs)
            return first;
        }
    }
}

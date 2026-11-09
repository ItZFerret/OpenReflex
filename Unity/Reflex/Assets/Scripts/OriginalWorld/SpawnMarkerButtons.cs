// SPAWN MARKERS' two controls on one device (w-markerbtns; OriginalSpawnMarker owns one for the keyboard and one for the pad). Plain C#, no
// Unity: the frame's inputs and clock go in, what to do comes out (tests/spawn_marker_buttons/ runs it headless).
//   two inputs  SET arms on its press and sets the marker once held HoldSeconds (released sooner: Hint, how to set it); RIDE rides back on its
//               press (at once: no release wait).
//   one input   (both rows on the same input, the pre-w-markerbtns control) held HoldSeconds = Set, released sooner = Ride.
//   blocked     a press that starts while blocked (a menu, a race: the inputs are the game's there) never counts, nor does one held
//               into the block; held on after the block ends it still does not count (only a new press does). The press that ends a block
//               (B closing the pause page in the frame the page closes) does not count either: while blocked both inputs are latched as held.
using System;

namespace Reflex.Port.OriginalWorld
{
    public sealed class SpawnMarkerButtons
    {
        [Flags] public enum Act { None = 0, Set = 1, Ride = 2, Hint = 4 }
        bool setWas, rideWas, armed, fired; float downAt;

        public Act Tick(float now, bool setDown, bool rideDown, bool same, bool blocked, float holdSeconds)
        {
            Act act = Act.None;
            if (blocked)
            {   // both inputs count as held while blocked: a press must start after the block. The press that ENDS the block (B closing the
                // pause page, which may read it before this runs in the same frame) is seen here with the block already gone; latching
                // them here makes it a press that was already down, so it never rides back to the marker.
                armed = false; setWas = rideWas = true;
                return act;
            }
            if (setDown && !setWas) { armed = true; fired = false; downAt = now; }
            if (armed && setDown && !fired && now - downAt >= holdSeconds) { fired = true; act |= Act.Set; }
            if (!setDown && setWas)
            {
                bool tap = armed && !fired;
                armed = false;
                if (tap) act |= same ? Act.Ride : Act.Hint;
            }
            setWas = setDown;
            if (!same)
            {
                if (rideDown && !rideWas) act |= Act.Ride;
                rideWas = rideDown;
            }
            else rideWas = setDown;
            return act;
        }

        /// A new world / markers off: nothing armed, and an input still held counts as already pressed (it must be let go and pressed again).
        public void Cancel() { armed = fired = false; setWas = rideWas = true; }
    }
}

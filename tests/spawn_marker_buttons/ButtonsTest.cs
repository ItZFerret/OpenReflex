// tests/spawn_marker_buttons: SpawnMarkerButtons (the SPAWN MARKERS controls' state machine, w-markerbtns) driven frame by frame at 60 Hz.
// run.sh compiles it with Unity's bundled csc against the one source file (no Unity) and runs it with the bundled dotnet. Exit 0 = PASS.
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using Act = Reflex.Port.OriginalWorld.SpawnMarkerButtons.Act;

static class ButtonsTest
{
    const float Hold = 0.6f, Dt = 1f / 60f;
    static int failures;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "ok   " : "FAIL ") + what); if (!ok) failures++; }

    // a script: per frame (set, ride, blocked); returns the frames that acted
    static List<(int frame, Act act)> Run(bool same, Func<int, (bool set, bool ride, bool blocked)> input, int frames, bool cancelFirst = false)
    {
        var b = new SpawnMarkerButtons(); var acts = new List<(int, Act)>();
        if (cancelFirst) b.Cancel();
        for (int f = 0; f < frames; f++)
        {
            var i = input(f);
            var a = b.Tick(f * Dt, i.set, i.ride, same, i.blocked, Hold);
            if (a != Act.None) acts.Add((f, a));
        }
        return acts;
    }
    static string Show(List<(int frame, Act act)> l) { var s = new List<string>(); foreach (var x in l) s.Add(x.act + "@" + x.frame); return s.Count == 0 ? "nothing" : string.Join(", ", s); }

    static int Main()
    {
        // two inputs (Y = SET, B = RIDE)
        var a = Run(false, f => (f < 42, false, false), 90);
        Check(a.Count == 1 && a[0].act == Act.Set && a[0].frame == 36, "hold Y 0.7 s: one Set at 0.6 s (frame 36): " + Show(a));
        a = Run(false, f => (f < 12, false, false), 60);
        Check(a.Count == 1 && a[0].act == Act.Hint && a[0].frame == 12, "tap Y 0.2 s: a Hint when it is let go, no Set: " + Show(a));
        a = Run(false, f => (false, f >= 5 && f < 40 || f >= 50 && f < 55, false), 90);
        Check(a.Count == 2 && a[0] == (5, Act.Ride) && a[1] == (50, Act.Ride), "press B: Ride on the press frame (no release wait), held B does not repeat, a new press rides again: " + Show(a));
        a = Run(false, f => (f < 42, f == 20, false), 90);
        Check(a.Count == 2 && a[0] == (20, Act.Ride) && a[1] == (36, Act.Set), "B pressed while Y is held: Ride at once, the hold still sets: " + Show(a));
        // one input on both rows (the pre-w-markerbtns control)
        a = Run(true, f => (f < 42, f < 42, false), 90);
        Check(a.Count == 1 && a[0] == (36, Act.Set), "one input, held 0.7 s: Set only: " + Show(a));
        a = Run(true, f => (f < 12, f < 12, false), 60);
        Check(a.Count == 1 && a[0] == (12, Act.Ride), "one input, tapped: Ride when let go: " + Show(a));
        // blocked (a menu, a race, the chat)
        a = Run(false, f => (false, f >= 5 && f < 40, f < 10), 60);
        Check(a.Count == 0, "B pressed while blocked and held after the block: nothing: " + Show(a));
        a = Run(false, f => (false, f == 3 || f == 4 || f == 20, f < 4), 60);
        Check(a.Count == 1 && a[0] == (20, Act.Ride), "the online chat closes on B (frame 3, the next frame still blocked); held on: nothing; a new press rides: " + Show(a));
        a = Run(false, f => (false, f >= 10 && f < 16 || f == 40, f < 10), 60);
        Check(a.Count == 1 && a[0] == (40, Act.Ride), "B closes the pause page (pressed on the first frame after the block: the page read it first): no ride back; a new press rides: " + Show(a));
        a = Run(false, f => (f >= 10 && f < 60, false, f < 10), 90);
        Check(a.Count == 0, "Y pressed on the frame a menu closes and held: no Set (a new press sets): " + Show(a));
        a = Run(false, f => (f < 60, false, f >= 20 && f < 25), 90);
        Check(a.Count == 0, "Y held into a block: no Set (and no Hint): " + Show(a));
        // a new world with the inputs still held
        a = Run(false, f => (f < 50, f < 30, false), 90, cancelFirst: true);
        Check(a.Count == 0, "inputs held through Cancel (a new world): nothing until pressed again: " + Show(a));
        Console.WriteLine(failures == 0 ? "PASS" : "FAILED");
        return failures == 0 ? 0 : 1;
    }
}

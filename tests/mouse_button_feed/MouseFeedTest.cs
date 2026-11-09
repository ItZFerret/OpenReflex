// tests/mouse_button_feed: MouseButtonFeed (the mouse buttons the client hands the game, w-lookback) driven frame by frame, into a model of the
// game's own Raw Input accumulator (Mouse_AccumulateRawInput 00817cb0: held = (held | down) & ~up, the flags of a game frame OR-ed together as
// OpenReflexSimulation.AddMouse does; no game frame runs while the client is paused, the flags wait for the next one). The LEFT button the game
// holds is the stock camera's look back. The client's old logic (the feed off: the button changes of those frames dropped) runs next to it.
// run.sh compiles it with Unity's bundled csc against the one source file (no Unity) and runs it with the bundled dotnet. Exit 0 = PASS.
using System;
#pragma warning disable CS0649   // (the script structs set only the fields a scenario uses)
using Reflex.Port.OriginalWorld;

static class MouseFeedTest
{
    static int failures;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "ok   " : "FAIL ") + what); if (!ok) failures++; }

    // the client's ReadMouse before w-lookback
    sealed class OldFeed
    {
        bool l, r, c;
        public int Tick(bool left, bool right, bool middle, bool feed)
        {
            if (!feed) { l = left; r = right; c = middle; return 0; }
            int f = 0;
            if (left != l) f |= left ? 0x1 : 0x2; if (right != r) f |= right ? 0x4 : 0x8; if (middle != c) f |= middle ? 0x10 : 0x20;
            l = left; r = right; c = middle; return f;
        }
    }

    // the game: held bits from the flags delivered with each game frame
    sealed class Game
    {
        public int held, pending;
        public void Add(int flags) => pending |= flags;
        public void Frame()
        {
            int down = 0, up = 0;
            for (int i = 0; i < 3; i++) { down |= ((pending >> (2 * i)) & 1) << i; up |= ((pending >> (2 * i + 1)) & 1) << i; }
            held = (held | down) & ~up; pending = 0;
        }
        public bool Left => (held & 1) != 0;
    }

    struct F { public bool l, r, c, feed, frames, newWorld; }
    // a script of Unity frames -> the game's held bits after each frame, for the new and the old client
    static (int[] now, int[] old) Run(int n, Func<int, F> script)
    {
        var feed = new MouseButtonFeed(); var old = new OldFeed(); var g = new Game(); var go = new Game();
        var a = new int[n]; var b = new int[n];
        for (int i = 0; i < n; i++)
        {
            var f = script(i);
            if (f.newWorld) { feed.Reset(f.l, f.r, f.c); g = new Game(); go = new Game(); }
            g.Add(feed.Tick(f.l, f.r, f.c, f.feed)); go.Add(old.Tick(f.l, f.r, f.c, f.feed));
            if (f.frames) { g.Frame(); go.Frame(); }
            a[i] = g.held; b[i] = go.held;
        }
        return (a, b);
    }
    static F Ride(bool l) => new F { l = l, feed = true, frames = true };

    static int Main()
    {
        // 1 riding: left down 30 frames, up: the game looks back while held, ahead after
        var (n, o) = Run(90, i => Ride(i >= 10 && i < 40));
        Check((n[20] & 1) == 1 && n[60] == 0, "riding: left held -> the game holds it, released -> it lets go");

        // 2 the report: left down while riding, the pause (feed off, no game frames) at 30, let go at 40 inside the pause, resume at 60
        Func<int, F> pause = i => new F { l = i >= 10 && i < 40, feed = i < 30 || i >= 60, frames = i < 30 || i >= 60 };
        (n, o) = Run(150, pause);
        Check((o[149] & 1) == 1, "old client: the UP was dropped in the pause -> the game holds LEFT for good (stuck looking back): held " + o[149]);
        Check(n[29] == 1 && n[60] == 0 && n[149] == 0, "new: the game lets go with the first frame after the pause, and stays released: held " + n[60] + " / " + n[149]);

        // 3 a menu that keeps the game frames running (feed off, frames on): released at once, not when the button comes up
        (n, o) = Run(120, i => new F { l = i >= 10 && i < 80, feed = i < 30 || i >= 90, frames = true });
        Check(n[29] == 1 && n[30] == 0 && n[119] == 0, "menu with frames: the game lets go on the menu's first frame (held at 29: " + n[29] + ", at 30: " + n[30] + ")");
        Check((o[119] & 1) == 1, "old client: stuck after the menu too");

        // 4 the menu closes with the button still held (pressed in the menu, or held through it): no DOWN until it is let go and pressed again
        (n, o) = Run(150, i => new F { l = i >= 10 && i < 100 || i >= 120 && i < 130, feed = i < 30 || i >= 60, frames = true });
        Check(n[70] == 0 && n[110] == 0, "held out of the menu: the game does not hold it (" + n[70] + ")");
        Check(n[125] == 1 && n[140] == 0, "a new press after it: held, then released");

        // 5 a new world while held: nothing owed, no DOWN until a new press
        (n, o) = Run(100, i => { var f = Ride(i >= 10 && i < 50); f.newWorld = i == 30; return f; });
        Check(n[35] == 0 && n[99] == 0, "a new world with LEFT held: the new game holds nothing");

        // 6 the mouse unplugged while held (ReadMouse: m == null -> all up, feed off)
        (n, o) = Run(80, i => i < 40 ? Ride(i >= 10) : new F { feed = false, frames = true });
        Check(n[39] == 1 && n[40] == 0, "mouse gone while held: the game lets go");

        // 7 right and middle the same (DOWN 0x4 / 0x10, UP 0x8 / 0x20), each released once when the feed stops
        var feed = new MouseButtonFeed();
        int fl = feed.Tick(false, true, true, true);
        Check(fl == (0x4 | 0x10), "right + middle down: 0x14 (" + fl.ToString("x") + ")");
        fl = feed.Tick(false, true, true, false);
        Check(fl == (0x8 | 0x20) && !feed.GameHolds(1) && !feed.GameHolds(2), "feed off: their UPs 0x28 (" + fl.ToString("x") + ")");
        fl = feed.Tick(false, false, false, false);
        Check(fl == 0, "and only once (" + fl.ToString("x") + ")");

        // 8 the Dynamic FPV free look takes the mouse while held and gives it back: released when it takes it
        (n, o) = Run(120, i => new F { l = i >= 10 && i < 70, feed = !(i >= 30 && i < 50), frames = true });
        Check(n[29] == 1 && n[30] == 0 && n[60] == 0 && n[119] == 0, "free look took the mouse while LEFT was held: released, no DOWN when it gives it back");

        Console.WriteLine(failures == 0 ? "PASS" : "FAIL (" + failures + ")");
        return failures == 0 ? 0 : 1;
    }
}

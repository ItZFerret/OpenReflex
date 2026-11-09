// TEST ONLY (w-fb-hud): Content/ReflexShuffleBag.cs outside Unity (tests/music_bag/bag_test.sh). Prints "ok ..." / "FAIL ..." and
// "BAG PASS (n checks)" or "BAG FAIL"; exit 0 on pass.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;

static class BagTests
{
    static int checks, fails;
    static void Check(bool ok, string what) { checks++; if (!ok) fails++; Console.WriteLine((ok ? "ok   " : "FAIL ") + what); }

    static int Main()
    {
        WholeRounds();
        Playlists();
        LevelsShareTheRound();
        TwoEntries();
        MenuThemes();
        Seeds();
        CountChange();
        Edge();
        AgainstTheGame();
        Console.WriteLine(fails == 0 ? $"BAG PASS ({checks} checks)" : $"BAG FAIL ({fails} of {checks})");
        return fails == 0 ? 0 : 1;
    }

    // the whole list, no filter: every 24 picks are a permutation, and a round never starts with the last pick
    static void WholeRounds()
    {
        var b = new ReflexShuffleBag(1234); const int n = 24, rounds = 200;
        var picks = new List<int>();
        for (int i = 0; i < n * rounds; i++) picks.Add(b.Next(n));
        bool perms = true, boundaries = true, repeats = false;
        for (int r = 0; r < rounds; r++)
        {
            var seg = picks.Skip(r * n).Take(n).ToList();
            if (seg.Distinct().Count() != n || seg.Min() != 0 || seg.Max() != n - 1) perms = false;
            if (r > 0 && seg[0] == picks[r * n - 1]) boundaries = false;
        }
        for (int i = 1; i < picks.Count; i++) if (picks[i] == picks[i - 1]) repeats = true;
        Check(perms, $"24 tracks, {rounds} rounds: each round of 24 picks plays every track once");
        Check(boundaries, "a new round never starts with the track that played last");
        Check(!repeats, $"no track twice in a row in {picks.Count} picks");
        Check(b.Rounds == rounds, $"rounds counted ({b.Rounds})");
        // the order is not one fixed order: the rounds differ
        var firsts = Enumerable.Range(0, rounds).Select(r => string.Join(",", picks.Skip(r * n).Take(n))).Distinct().Count();
        Check(firsts == rounds, $"{firsts} different round orders of {rounds}");
    }

    // a level's playlist (13 of the 24): every 13 picks are its 13 tracks
    static void Playlists()
    {
        var b = new ReflexShuffleBag(77); const int n = 24;
        var allowed = new HashSet<int> { 0, 2, 3, 5, 7, 8, 11, 12, 15, 17, 19, 21, 23 };
        var picks = new List<int>();
        for (int i = 0; i < 13 * 50; i++) picks.Add(b.Next(n, allowed));
        bool inside = picks.All(allowed.Contains), perms = true, adj = true;
        for (int r = 0; r < 50; r++) if (picks.Skip(r * 13).Take(13).Distinct().Count() != 13) perms = false;
        for (int i = 1; i < picks.Count; i++) if (picks[i] == picks[i - 1]) adj = false;
        Check(inside, "a level's playlist: only its tracks play");
        Check(perms, "every 13 picks of a 13-track playlist are its 13 tracks (no repeat before all played)");
        Check(adj, "no track twice in a row across the playlist's rounds");
    }

    // races on different levels (overlapping playlists, 3 songs a race): within one round of the shared bag no track repeats, and a level's
    // songs are not the same picks race after race
    static void LevelsShareTheRound()
    {
        var b = new ReflexShuffleBag(4242); const int n = 24;
        var A = new HashSet<int>(Enumerable.Range(0, 13)); var B = new HashSet<int>(Enumerable.Range(8, 13));
        var history = new List<(int round, int pick)>();
        var firstOfRaceA = new List<int>();
        for (int race = 0; race < 60; race++)
        {
            var set = race % 2 == 0 ? A : B;
            for (int k = 0; k < 3; k++) { int p = b.Next(n, set); if (k == 0 && set == A) firstOfRaceA.Add(p); history.Add((b.Rounds, p)); }
        }
        bool noRepeatInRound = history.GroupBy(h => h.round).All(g => g.Select(h => h.pick).Distinct().Count() == g.Count());
        Check(noRepeatInRound, "levels with overlapping playlists share one round: no track repeats inside a round");
        Check(firstOfRaceA.Distinct().Count() >= 8, $"level A's first song over 30 races: {firstOfRaceA.Distinct().Count()} different (the game's per-session pick: always the same)");
        bool adj = true; for (int i = 1; i < history.Count; i++) if (history[i].pick == history[i - 1].pick) adj = false;
        Check(adj, "no track twice in a row across the races");
    }

    static void TwoEntries()
    {
        var b = new ReflexShuffleBag(9);
        var p = Enumerable.Range(0, 40).Select(_ => b.Next(2)).ToList();
        bool alt = true; for (int i = 1; i < p.Count; i++) if (p[i] == p[i - 1]) alt = false;
        Check(alt, "two entries alternate (a round never starts with the last one)");
        var one = new ReflexShuffleBag(9);
        Check(Enumerable.Range(0, 5).All(_ => one.Next(1) == 0), "one entry: always it");
    }

    // the menus' three themes: the menu bag, apart from the race's
    static void MenuThemes()
    {
        ReflexMusicBags.Reseed(555);
        var menu = ReflexMusicBags.Get(ReflexMusicBags.Menu); var race = ReflexMusicBags.Get(ReflexMusicBags.Race);
        Check(!ReferenceEquals(menu, race), "menu and race keep their own bags");
        Check(ReferenceEquals(menu, ReflexMusicBags.Get(ReflexMusicBags.Menu)), "a context's bag lives for the launch (the same object across scenes)");
        var themes = new List<int>();
        for (int i = 0; i < 30; i++) { themes.Add(menu.Next(3)); race.Next(24); }
        bool adj = true; for (int i = 1; i < themes.Count; i++) if (themes[i] == themes[i - 1]) adj = false;
        bool perms = true; for (int r = 0; r < 10; r++) if (themes.Skip(r * 3).Take(3).Distinct().Count() != 3) perms = false;
        Check(adj, "back in the menus: never the theme of the last time");
        Check(perms, "the 3 themes each once before any again (race picks in between do not disturb it)");
    }

    static void Seeds()
    {
        var b1 = new ReflexShuffleBag(31); var b2 = new ReflexShuffleBag(31); var b3 = new ReflexShuffleBag(32);
        var s1 = Enumerable.Range(0, 48).Select(_ => b1.Next(24)).ToList();
        var s2 = Enumerable.Range(0, 48).Select(_ => b2.Next(24)).ToList();
        var s3 = Enumerable.Range(0, 48).Select(_ => b3.Next(24)).ToList();
        Check(s1.SequenceEqual(s2), "the same seed: the same order (a launch is repeatable for tests)");
        Check(!s1.SequenceEqual(s3), "another seed (another launch): another order");
        ReflexMusicBags.Reseed(1); var g1 = Enumerable.Range(0, 24).Select(_ => ReflexMusicBags.Get(ReflexMusicBags.Race).Next(24)).ToList();
        ReflexMusicBags.Reseed(2); var g2 = Enumerable.Range(0, 24).Select(_ => ReflexMusicBags.Get(ReflexMusicBags.Race).Next(24)).ToList();
        Check(!g1.SequenceEqual(g2), "the launch seed decides the race bag's order");
    }

    static void CountChange()
    {
        var b = new ReflexShuffleBag(3);
        for (int i = 0; i < 5; i++) b.Next(24);
        int r0 = b.Rounds; int p = b.Next(10);
        Check(p >= 0 && p < 10 && b.Rounds == r0 + 1, "the list changed size: a new round of the new list");
    }

    static void Edge()
    {
        var b = new ReflexShuffleBag(3);
        Check(b.Next(0) == -1, "an empty list: -1");
        Check(b.Next(5, new HashSet<int> { 7, 9 }) == -1, "nothing allowed in range: -1");
        Check(b.Next(5, new HashSet<int> { 4 }) == 4 && b.Next(5, new HashSet<int> { 4 }) == 4, "a one-track playlist plays it every time");
    }

    // what players heard before (the game's pick, its generator starting the same every race session) against the bag over 20 races
    static void AgainstTheGame()
    {
        const int n = 24; var level = new HashSet<int>(Enumerable.Range(0, 13));
        var game = new List<int>(); for (int race = 0; race < 20; race++) game.Add(new ReflexShuffleBag(20261104).Next(n, level));
        var bag = new ReflexShuffleBag(20261104); var mine = new List<int>(); for (int race = 0; race < 20; race++) mine.Add(bag.Next(n, level));
        Console.WriteLine($"     first song of 20 races on one level: per-session pick {game.Distinct().Count()} different, the launch bag {mine.Distinct().Count()} different");
        Check(game.Distinct().Count() == 1 && mine.Distinct().Count() == 13, "a fresh generator per race repeats the first song; the bag cycles through all 13");
    }
}

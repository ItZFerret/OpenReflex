// The music's shuffle bags (w-fb-hud; a playtester: "randomize the music order better, so players aren't hearing the same songs on repeat").
// No UnityEngine: tests/music_bag/bag_test.sh compiles this file alone.
//
// Why the songs repeated: the race's jukebox is the game's (core game/music_player.c): each race hands it the level's track list, shuffled
// with the game's own generator, and every race is a new session whose generator starts from the same seed, so the same level played the
// same songs in the same order every time, and its first song was the same pick every race. The core keeps those draws (the rest of the
// game's randomness follows them), so the host chooses what plays: the game still says WHEN a track starts and ends (its delay, fades,
// next-entry requests), a bag says WHICH one.
//
// A bag is one round of a context's whole list in a Fisher-Yates order (a generator seeded per launch): every entry plays once before any
// entry plays again, and a new round never starts with the entry that played last. Next(count, allowed) takes the first entry of the round
// that `allowed` contains (a level's playlist: 13 of the 24 race tracks; null = all), so a level's songs come from a round shared by every
// level and race of the launch; when the round has no allowed entry left a new round begins. A change of `count` (the list changed) starts
// a new round too. The bags live for the process (across races, scenes and menus) and are kept per context: "race" (the in-race
// soundtrack: OriginalMusic), "menu" (the menus' UI themes: FrontEndSounds FeUiMusic), "race-offline" (OriginalMusic's own shuffle when
// no game music player runs).
using System;
using System.Collections.Generic;

namespace Reflex.Port.Content
{
    public sealed class ReflexShuffleBag
    {
        readonly Random rnd;
        readonly List<int> round = new List<int>();
        int count = -1;
        /// The entry Next returned last (-1 none yet).
        public int Last { get; private set; } = -1;
        /// Rounds begun (tests).
        public int Rounds { get; private set; }
        /// The entries left in this round, in order (tests).
        public IReadOnlyList<int> Remaining => round;

        public ReflexShuffleBag(int seed) { rnd = new Random(seed); }

        /// The next entry of 0 .. count - 1 (-1 when count < 1 or nothing is allowed); allowed: the entries this context may play now (null = all).
        public int Next(int count, ICollection<int> allowed = null)
        {
            if (count < 1) return -1;
            if (allowed != null)
            {
                bool any = false;
                foreach (int a in allowed) if (a >= 0 && a < count) { any = true; break; }
                if (!any) return -1;
            }
            if (count != this.count) { this.count = count; round.Clear(); }
            int at = Find(allowed);
            if (at < 0) { NewRound(allowed); at = Find(allowed); }
            int pick = round[at];
            round.RemoveAt(at);
            Last = pick;
            return pick;
        }

        int Find(ICollection<int> allowed)
        {
            for (int i = 0; i < round.Count; i++) if (allowed == null || allowed.Contains(round[i])) return i;
            return -1;
        }

        // Fisher-Yates over the whole list; the round's first allowed entry is never the last one played (when another is allowed)
        void NewRound(ICollection<int> allowed)
        {
            round.Clear();
            for (int i = 0; i < count; i++) round.Add(i);
            for (int i = count - 1; i > 0; i--) { int j = rnd.Next(i + 1); (round[i], round[j]) = (round[j], round[i]); }
            Rounds++;
            int first = Find(allowed);
            if (first < 0 || round[first] != Last) return;
            for (int k = first + 1; k < round.Count; k++)
                if (allowed == null || allowed.Contains(round[k])) { (round[first], round[k]) = (round[k], round[first]); return; }
        }
    }

    /// The bags of the launch, one per context (see the header). The seed is the launch's (the clock and the process): another launch, another order.
    public static class ReflexMusicBags
    {
        public const string Race = "race", Menu = "menu", RaceOffline = "race-offline";
        static readonly Dictionary<string, ReflexShuffleBag> bags = new Dictionary<string, ReflexShuffleBag>();
        static int seed = Environment.TickCount ^ (int)(DateTime.UtcNow.Ticks >> 8) ^ Guid.NewGuid().GetHashCode();
        /// The launch's seed (tests: Reseed makes a run repeatable).
        public static int Seed => seed;
        public static ReflexShuffleBag Get(string context)
        {
            lock (bags)
            {
                if (!bags.TryGetValue(context, out var b)) bags[context] = b = new ReflexShuffleBag(unchecked(seed + StableHash(context)));
                return b;
            }
        }
        /// Forgets every bag and seeds the next ones with `value` (tests).
        public static void Reseed(int value) { lock (bags) { bags.Clear(); seed = value; } }
        static int StableHash(string s) { unchecked { int h = 17; foreach (char c in s) h = h * 31 + c; return h; } }
    }
}

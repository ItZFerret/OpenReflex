using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// The medal criteria of a Free Ride level's challenges, read from the level's scene file as the game's MXChallengesSpecEntity is
    /// (core/game/build_scene_activity.c rx_challenge_specs_vf5 0040fc10: ChallengeSpecsRSData in "<level>Base"), for the front end's Race &amp; Ride
    /// challenges page - the numbers PreChallengePopup lists (App.GetChallengeAwardCriteria) before the challenge is ridden. The A variant (track 0) is
    /// the one the EventTypeLocations rows start:
    ///   Hill Climb      HillClimbBronze / Silver / GoldHeight (feet; the gold default is 100 as the entity's)
    ///   Trailblazer     TrailblazerABronze / Silver / GoldTime (seconds, lower is better)
    ///   Target Practice TargetPracticeBronze / Silver / GoldTargets (hits)
    ///   Hit Discovery   no criteria in the entity: MXActivityFreeRide's discovery award (00585330) gives bronze at a third of the level's
    ///                   collectibles (CollectibleHitDiscoveryRSData in "<level>FreeRide"), silver at two thirds, gold at all of them.
    /// Values are floats (bronze, silver, gold); null when the level does not carry them.
    public sealed class ReflexChallengeSpecs
    {
        public float[] hillClimb, trailblazer, targets, discoveries;
        public int discoveryTotal;

        static readonly Dictionary<string, ReflexChallengeSpecs> cache = new Dictionary<string, ReflexChallengeSpecs>(StringComparer.OrdinalIgnoreCase);
        static readonly object gate = new object();

        /// The criteria of a level (cached); an empty set when the scene file cannot be read.
        public static ReflexChallengeSpecs For(string level)
        {
            if (string.IsNullOrEmpty(level)) return new ReflexChallengeSpecs();
            lock (gate)
            {
                if (cache.TryGetValue(level, out var c)) return c;
                var s = new ReflexChallengeSpecs();
                try { s.Load(level); }
                catch (Exception e) { Debug.LogWarning("[Challenges] criteria of " + level + ": " + e.Message); }
                return cache[level] = s;
            }
        }

        void Load(string level)
        {
            string file = TrackMods.LevelFile(level, ".dx9.scene");
            if (!RxFiles.Exists(file)) throw new FileNotFoundException(file);
            var data = RxFiles.ReadAllBytes(file);
            var names = ReflexScene.SectionNames(data);
            string baseSection = names.FirstOrDefault(n => n.EndsWith("Base", StringComparison.Ordinal)) ?? names[0];
            string stem = baseSection.EndsWith("Base", StringComparison.Ordinal) ? baseSection.Substring(0, baseSection.Length - 4) : baseSection;
            var wanted = new List<string> { baseSection };
            if (names.Contains(stem + "FreeRide")) wanted.Add(stem + "FreeRide");
            var entities = ReflexScene.Read(data, wanted.ToArray());
            var spec = entities.FirstOrDefault(e => e.Type == "ChallengeSpecsRSData");
            if (spec != null)
            {
                hillClimb = new[] { spec.Float("HillClimbBronzeHeight"), spec.Float("HillClimbSilverHeight"), spec.Has("HillClimbGoldHeight") ? spec.Float("HillClimbGoldHeight") : 100f };
                trailblazer = new[] { spec.Float("TrailblazerABronzeTime"), spec.Float("TrailblazerASilverTime"), spec.Float("TrailblazerAGoldTime") };
                if (spec.Has("TargetPracticeBronzeTargets"))
                    targets = new float[] { spec.Int("TargetPracticeBronzeTargets"), spec.Int("TargetPracticeSilverTargets"), spec.Int("TargetPracticeGoldTargets") };
            }
            discoveryTotal = entities.Count(e => e.Type == "CollectibleHitDiscoveryRSData");
            if (discoveryTotal > 0)
            {
                int third = discoveryTotal / 3;   // 00585330: n * 0x55555556 >> 32, the sign added
                discoveries = new float[] { third, third * 2, discoveryTotal };
            }
        }
    }
}

using System;

namespace Reflex.Port.RxCore
{
    /// MAP SCALE (port addition, core/plan/MAP_SCALE.md): the scale the next / current exhibition world is made at. The race setup's
    /// MAP SCALE row picks it per track (ReflexSettings.MapScalePercent); ReflexEventSelect.Launch sets Current before the event scene
    /// loads, so every file-built part of the world (scenery, forest, water, sounds ...) is made at it, and OpenReflexSimulation hands it to
    /// the core (rx_session_set_map_scale) before the event loads. 1 everywhere else (career, MotoSkills, test ride, challenges, the front
    /// end's menu world).
    ///
    /// BASE SCALE (w-alivegates, core/plan/MAP_SCALE.md "Base scale"): a track can have a default scale of its own (Base: the converted MX vs
    /// ATV Alive race levels, Nationals and short tracks 110 %, Supercross 130 %: Content/AliveLevels.MapScaleBase). The row's percent is
    /// relative to it (100 % = the track's default), so the world's scale is Base x percent; the track counts as unscaled for its records
    /// and PRACTICE laps at its default (OffDefault false), and the core is told the base (rx_session_set_map_scale_base).
    public static class RxMapScale
    {
        public const int MinPercent = 50, MaxPercent = 200, StepPercent = 5, DefaultPercent = 100;

        /// The world's scale (1 = the game's track size). Main thread.
        public static float Current { get; private set; } = 1f;
        /// The track's default scale (1 for the game's tracks).
        public static float Base { get; private set; } = 1f;
        /// The world is not at the game's size (the file-built parts are scaled).
        public static bool Scaled => Current != 1f;
        /// The world is not at its track's default scale: no offline records, best laps, highlights or PRACTICE laps.
        public static bool OffDefault => Math.Abs(Current - Base) > 1e-4f * Base;

        public static int ClampPercent(int p)
        {
            p = (int)Math.Round(p / (double)StepPercent) * StepPercent;
            return p < MinPercent ? MinPercent : p > MaxPercent ? MaxPercent : p;
        }
        /// A row percent of the game's size (a track without a base).
        public static float FromPercent(int p) => FromPercent(p, 1f);
        /// A row percent on a track whose default scale is `baseScale`: the world's scale (100 % is the base itself).
        public static float FromPercent(int p, float baseScale)
        {
            baseScale = ValidBase(baseScale);
            return p == DefaultPercent ? baseScale : baseScale * (ClampPercent(p) / 100f);
        }
        /// A scale in percent (110 for 1.1).
        public static int Percent(float scale) => (int)Math.Round(scale * 100.0);

        /// Sets the scale of the world about to load and its track's default (ReflexEventSelect.Launch / the career, test ride and tutorial
        /// launches with 1). A scale outside the row's range about the base (50 .. 200 % of it) falls back to the base.
        public static void Set(float scale, float baseScale = 1f)
        {
            baseScale = ValidBase(baseScale);
            if (!(scale >= baseScale * MinPercent / 100f - 1e-4f && scale <= baseScale * MaxPercent / 100f + 1e-4f) || Math.Abs(scale - baseScale) < 1e-4f) scale = baseScale;
            Current = Math.Abs(scale - 1f) < 1e-4f ? 1f : scale;
            Base = baseScale;
        }
        public static void Reset() { Current = 1f; Base = 1f; }
        static float ValidBase(float b) => b >= 0.5f && b <= 2f && Math.Abs(b - 1f) >= 1e-4f ? b : 1f;

        /// "100 %", "135 %".
        public static string Label(int percent) => ClampPercent(percent) + " %";
    }
}

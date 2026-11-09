// TEST ONLY: the MODERN HUD's wreck-recovery prompt in the showcase (ReflexHudView.Reflex.cs). Each page builds ReflexHudView on a procedural
// backdrop and feeds its ReflexWreckHud SYNTHETIC rx_session_reflex_hud states (no race is ridden, nothing is read from the game; the control
// named on the cap is passed in, so the player's own remap table is not read). Pages:
// The prompt is the original layer's arrow (ReflexArrowMiddle.png tiles) with a small control chip; pages are shot at the full point of the
// arrow's 0.2 s pulse, -in at the start of it.
//   hud-reflex-up / -right / -down / -left   the prompt (the arrow key cap of the keyboard)       hud-reflex-up-in   early in the pulse
//   hud-reflex-mid / -late                    later in the window, the right stick / a remapped left arrow (the arrow does not change)
//   hud-reflex-lstick / -key / -tilt          the control chip for a left-handed pad, a remapped key, the motion sensor
//   hud-reflex-success / hud-reflex-fail      the frame after a save / a fall: the arrow is gone at once (the original's vf11 hides it; no outcome, no fade)
//   hud-reflex-race                           a race HUD around the prompt (the position, the gauge)
using System;
using System.Collections;
using System.Reflection;
using Reflex.Port.Hud;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class HudShowcaseReflex
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            int up = RxReflexHudState.DirUp, right = RxReflexHudState.DirRight, down = RxReflexHudState.DirDown, left = RxReflexHudState.DirLeft;
            UiShowcase.Register("hud-reflex-up", () => Page("hud-reflex-up", Prompt(up, 0.88f, 0f, "UpArrow")));
            UiShowcase.Register("hud-reflex-right", () => Page("hud-reflex-right", Prompt(right, 0.88f, 0f, "RightArrow")));
            UiShowcase.Register("hud-reflex-down", () => Page("hud-reflex-down", Prompt(down, 0.88f, 0f, "DownArrow")));
            UiShowcase.Register("hud-reflex-left", () => Page("hud-reflex-left", Prompt(left, 0.88f, 0f, "LeftArrow")));
            UiShowcase.Register("hud-reflex-up-in", () => Page("hud-reflex-up-in", Prompt(up, 0.88f, 0f, "UpArrow"), 0.07f));
            UiShowcase.Register("hud-reflex-mid", () => Page("hud-reflex-mid", Prompt(up, 0.5f, 0.6f, "R STICK")));
            UiShowcase.Register("hud-reflex-late", () => Page("hud-reflex-late", Prompt(left, 0.12f, 0.2f, "LeftArrow")));
            UiShowcase.Register("hud-reflex-lstick", () => Page("hud-reflex-lstick", Prompt(right, 0.7f, 0f, "L STICK")));
            UiShowcase.Register("hud-reflex-key", () => Page("hud-reflex-key", Prompt(down, 0.7f, 0f, "J")));
            UiShowcase.Register("hud-reflex-tilt", () => Page("hud-reflex-tilt", Prompt(up, 0.7f, 0f, "TILT")));
            UiShowcase.Register("hud-reflex-success", () => Page("hud-reflex-success", Outcome(up, 1, "R STICK"), 0.12f));
            UiShowcase.Register("hud-reflex-fail", () => Page("hud-reflex-fail", Outcome(right, 2, "RightArrow"), 0.10f));
            UiShowcase.Register("hud-reflex-race", () => Page("hud-reflex-race", Prompt(left, 0.62f, 0.15f, "LeftArrow"), 0.99f, true));
        }

        static RxReflexHudState Shown(int dir, float remaining, float flick, uint begin = 1, uint success = 0, uint fail = 0)
        {
            var r = RxReflexHudState.Create();
            r.valid = 1; r.shown = 1; r.direction = dir; r.litMask = dir == RxReflexHudState.DirUp ? 1u : dir == RxReflexHudState.DirRight ? 2u : dir == RxReflexHudState.DirDown ? 4u : 8u;
            r.mode = (uint)(dir == RxReflexHudState.DirUp ? 3 : dir == RxReflexHudState.DirDown ? 6 : dir == RxReflexHudState.DirLeft ? 9 : 12);
            r.timeMax = 0.9f; r.timeLeft = 0.9f * remaining; r.progress = flick * 0.25f; r.threshold = 0.25f;
            r.beginEvents = begin; r.successEvents = success; r.failEvents = fail;
            return r;
        }

        // a prompt held at this point of its window
        static Action<ReflexHudModel> Prompt(int dir, float remaining, float flick, string cap)
            => m => { m.wreck.Apply(Shown(dir, remaining, flick), true, Time.unscaledTime, cap); };

        // the prompt for a frame, then its outcome (the page runs on from that moment)
        static Action<ReflexHudModel> Outcome(int dir, int result, string cap)
            => m =>
            {
                m.wreck.Apply(Shown(dir, 0.3f, result == 1 ? 0.9f : 0f), true, Time.unscaledTime, cap);
                var after = Shown(dir, 0f, 0f, 1, result == 1 ? 1u : 0u, result == 2 ? 1u : 0u); after.shown = 0; after.litMask = 0; after.mode = 0; after.direction = 0; after.timeLeft = 0f;
                m.wreck.Apply(after, true, Time.unscaledTime, cap);
            };

        static IEnumerator Page(string name, Action<ReflexHudModel> fill, float settle = 0.39f, bool race = false)
        {
            var backdrop = HudShowcase.Backdrop();
            var view = ReflexHudView.Create(null, "HUD gallery " + name, 30001);
            view.Options.scale = 1f; view.Options.opacity = 1f; view.Options.standings = true; view.Options.gauge = true; view.Options.reduceMotion = false; view.Options.metric = false;
            var m = new ReflexHudModel();
            var baseFill = typeof(HudShowcase).GetMethod("Base", BindingFlags.NonPublic | BindingFlags.Static);
            baseFill.Invoke(null, new object[] { m, HudMode.Race, 3, 12, 3, 2 });
            if (!race) { m.standings.Clear(); m.MarkStandings(); view.Options.standings = false; view.Options.gauge = false; }
            fill(m);
            view.Visible = true;
            float t0 = Time.unscaledTime;
            while (Time.unscaledTime < t0 + settle) { view.Apply(m, Time.unscaledDeltaTime, 2); yield return null; }
            view.Apply(m, 0.0001f, 2);
            yield return UiShowcase.Shot(name);
            view.Destroy();
            UnityEngine.Object.Destroy(backdrop);
            ReflexHudKit.Scale = 1f; ReflexHudKit.PanelOpacity = 1f;
            yield return UiShowcase.Frames(3);
        }
    }
}

// OPTIONS > OPEN REFLEX > REPLAY: REPLAY / PHOTO MODE's recording (Reflex.Port.Replay.ReflexReplay, w-replayphoto). REPLAY LENGTH: the
// seconds kept (30 s .. 5 min); a field that would not fit the memory budget at 60 frames a second keeps 30. REPLAY EFFECTS: the
// particles (dust, roost, splashes) are recorded too (their own ring, 32 .. 128 MB). Both apply from the next race start.
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        void BuildReplayOptions(VisualElement panel)
        {
            Section(panel, "REPLAY & PHOTO MODE");
            var choices = Reflex.Port.Replay.ReflexReplay.LengthChoices;
            Named(OptUi.Spinner("REPLAY LENGTH", () => Reflex.Port.Replay.ReflexReplay.LengthLabel(Reflex.Port.Replay.ReflexReplay.LengthSeconds), d =>
            {
                int i = Mathf.Max(0, System.Array.IndexOf(choices, Reflex.Port.Replay.ReflexReplay.LengthSeconds));
                Reflex.Port.Replay.ReflexReplay.LengthSeconds = choices[(i + d + choices.Length) % choices.Length];
            }, panel), "replay-length");
            Named(OptUi.Toggle("REPLAY EFFECTS", () => Reflex.Port.Replay.ReflexReplay.RecordEffects, v => Reflex.Port.Replay.ReflexReplay.RecordEffects = v, panel), "replay-effects");
            Named(OptUi.Toggle("REPLAY SOUND", () => Reflex.Port.Replay.ReflexReplay.RecordSound, v => Reflex.Port.Replay.ReflexReplay.RecordSound = v, panel), "replay-sound");
            Named(OptUi.Toggle("CINEMATIC EFFECTS", () => Reflex.Port.Replay.ReflexReplay.CinematicEffects, v => Reflex.Port.Replay.ReflexReplay.CinematicEffects = v, panel), "replay-cinematic");
            Note(panel, "CINEMATIC EFFECTS (replays only, never while riding; PHOTO MODE has its own settings): depth of field that follows the rider, motion blur on the bikes, wheels and background, and subtle lens effects (chromatic aberration, vignette, film grain, lens flare and dirt on the sun and the lamps), set per camera. Ours.");
            Note(panel, "Every ride is recorded for REPLAY and PHOTO MODE (the pause menu; R / P while riding when the game's keys leave them free). " +
                        "A full 12-rider race takes about 70 MB a minute at 60 frames a second; a length that would not fit in an eighth of your memory keeps 30 frames a second. " +
                        "REPLAY EFFECTS keeps the dust, roost and splashes, the ruts as they were and the riders' mud too (up to ~300 MB more on a big machine). " +
                        "REPLAY SOUND plays the engines, landings, crashes, the gate and the crowd again at normal speed (0.75x to 1.5x; silent otherwise). Photos go to Pictures/Open Reflex. Applies from the next race start. Ours.");
        }
    }
}

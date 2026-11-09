// RIDER CAP (w-rc-client, core/plan/RIDER_CAP.md "Done (w-rc-client)"): FIELD SIZE, the riders of an offline race with the AI on, you
// included. One small piece any page can use (the race setup's EVENT tab today; a championship setup can call Row with its own getter /
// setter later), fed by RxRiderCap:
//   Cap(e)     the most riders an event offers: RxRiderCap.Max (32) for the lap races (National, Supercross, Terracross, Powercross ...;
//              the Alive races are Nationals / Supercross), the original's 12 for the modes whose own tables the core keeps at 12
//              (Waypoint, Freestyle, Section Race)
//   Clamp      a stored choice within 2 .. Cap(e); 12 (the original's full gate, the default) when unset
//   Row        the "FIELD SIZE  < 24 RIDERS >" spinner + its note: greyed with AI off ("YOU ALONE") and on the modes kept at 12; left /
//              right step one rider and wrap; the note warns on a Supercross field past RxRiderCap.SupercrossComfortable (advice only)
using System;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenFieldSize
    {
        /// The most riders this event's field can have.
        public static int Cap(ReflexEvents.Event e)
        {
            if (e == null) return RxRiderCap.Original;
            if (e.spec.type == "WaypointRace" || e.spec.type == "Freestyle" || e.spec.type == "SectionRace") return RxRiderCap.Original;
            return e.spec.durationType == "Laps" ? RxRiderCap.Max : RxRiderCap.Original;
        }
        /// The event's field can go past the original's 12.
        public static bool Opens(ReflexEvents.Event e) => Cap(e) > RxRiderCap.Original;
        /// A stored choice (0 / out of range: the original's 12) within the event's cap.
        public static int Clamp(ReflexEvents.Event e, int riders)
        {
            int n = riders >= 2 && riders <= RxRiderCap.Max ? riders : RxRiderCap.Original;
            return Math.Min(n, Cap(e));
        }
        /// A Supercross field past RxRiderCap.SupercrossComfortable (the tight tracks pile up): warned, never refused.
        public static bool Crowded(ReflexEvents.Event e, int riders) => e != null && e.spec.type == "Supercross" && Clamp(e, riders) > RxRiderCap.SupercrossComfortable;

        /// The row's value: "YOU ALONE", "12 RIDERS  ·  ORIGINAL", "24 RIDERS", "12 RIDERS  ·  THIS MODE'S MOST".
        public static string Value(ReflexEvents.Event e, bool aiOn, int riders)
        {
            if (!aiOn) return "YOU ALONE";
            int n = Clamp(e, riders);
            if (!Opens(e)) return n + " RIDERS  ·  THIS MODE'S MOST";
            return n + " RIDERS" + (n == RxRiderCap.Original ? "  ·  ORIGINAL" : "");
        }

        /// The note under the row.
        public static string Note(ReflexEvents.Event e, bool aiOn, int riders)
        {
            if (!aiOn) return "AI OPPONENTS is off: you ride alone. Turn the AI on to pick the field.";
            if (!Opens(e))
                return $"This mode keeps the original's field of {RxRiderCap.Original} (you and {RxRiderCap.OriginalOpponents} computer riders): its own tables were not opened past it.";
            int n = Clamp(e, riders);
            string t = $"You and {n - 1} computer riders. {RxRiderCap.Original} is the original game's full gate; up to {Cap(e)} riders line up behind the gate and ask more of your PC.";
            if (Crowded(e, riders))
                t += $"  SUPERCROSS: more than {RxRiderCap.SupercrossComfortable} riders pile up on the tight tracks (the rhythm sections are one bike wide): expect crashes and a straggler or two.";
            return t;
        }

        /// The FIELD SIZE row and its note on `parent`. get / set: the stored choice (set is called with the new rider count; the caller saves
        /// it); aiOn: whether the AI is on now. Returns the row (named `name`; the note is `name + "-note"`).
        public static VisualElement Row(VisualElement parent, ReflexEvents.Event e, Func<bool> aiOn, Func<int> get, Action<int> set, string name = "ou-setup-riders")
        {
            int cap = Cap(e);
            Label note = null;
            var row = OuUi.Spinner("FIELD SIZE", () => Value(e, aiOn(), get()), k =>
            {
                if (!aiOn() || cap <= RxRiderCap.Original) return;
                int n = Clamp(e, get()) + k;
                if (n > cap) n = 2; else if (n < 2) n = cap;
                set(n);
                if (note != null) note.text = Note(e, aiOn(), get());
            }, parent);
            row.name = name;
            bool off = !aiOn() || cap <= RxRiderCap.Original;
            row.style.opacity = off ? new StyleFloat(0.45f) : new StyleFloat(StyleKeyword.Null);
            note = OuUi.Text(Note(e, aiOn(), get()), "mxa-note", parent);
            note.name = name + "-note";
            return row;
        }
    }
}

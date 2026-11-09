using System;
using System.Globalization;
using Reflex.Port.RxCore;

namespace Reflex.Port.Hud
{
    internal static class WaypointHudText
    {
        internal static string Distance(float feet, bool metric)
        {
            if (float.IsNaN(feet) || float.IsInfinity(feet) || feet < 0 || feet >= 3.0e38f) return "—";
            return (feet * (metric ? 0.3048f : 1f)).ToString("0", CultureInfo.InvariantCulture) + (metric ? " m" : " ft");
        }
        internal static string Progress(bool available, RxWaypointState state, bool metric)
        {
            if (!available || state.gate_count <= 0) return "GATES PASSED  —";
            int passed = Math.Max(0, Math.Min(state.gate_count, state.gate_index + 1));
            return "GATES PASSED  " + passed + " / " + state.gate_count
                + (state.distance_valid != 0 ? "\nREMAINING  " + Distance(state.remaining_distance, metric) : "");
        }
    }
}

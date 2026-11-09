using System;

namespace Reflex.Port.RxCore
{
    // RIDER CAP (w-rc-client; core/plan/RIDER_CAP.md "Done (w-rc-client)"): how many riders the client is built for. The core's rx_api.h has
    // the same two numbers (RX_MAX_RIDERS 32, RX_ORIGINAL_RIDERS 12, w-rc-core; RxApiNative.MaxRiders / OriginalRiders); every C# array,
    // clamp and check sized by riders reads these instead of a literal 12 / 16. A race of Original or fewer riders looks and behaves exactly
    // as before: only races past it use the new rows, pages and level-of-detail rules.
    public static class RxRiderCap
    {
        /// The most riders in one race (you + 31): RX_MAX_RIDERS.
        public const int Max = RxApiNative.MaxRiders;
        /// The original game's field (you + 11): RX_ORIGINAL_RIDERS. Fields of this size or less are the original's, unchanged.
        public const int Original = RxApiNative.OriginalRiders;
        /// The most computer riders an offline event can ask for (Max - you).
        public const int MaxOpponents = Max - 1;
        /// The original's computer riders (a full gate of 12).
        public const int OriginalOpponents = Original - 1;
        /// Above this on a Supercross track the setup warns (RIDER_CAP.md: a headless 32-rider Kingston race left one back-row AI stuck for
        /// good; 24 finished). Advice, not a limit.
        public const int SupercrossComfortable = 24;

        /// A field past the original's (13+ riders).
        public static bool Big(int riders) => riders > Original;

        static int coreMax = -1, coreMaxGeneration = -1;

        /// The core's rider cap (rx_session_max_riders, clamped to Max); Original on a core without the export (before w-rc-core) or with
        /// no core loaded. Cached per library load.
        public static int CoreMax
        {
            get
            {
                if (coreMaxGeneration == RxNativeLoader.Generation && coreMax > 0) return coreMax;
                int v = Original;
                try { if (RxNativeLoader.IsLoaded) v = RxApiNative.rx_session_max_riders(); }
                catch (EntryPointNotFoundException) { v = Original; }
                catch (DllNotFoundException) { v = Original; }
                coreMax = Math.Max(1, Math.Min(Max, v)); coreMaxGeneration = RxNativeLoader.Generation;
                return coreMax;
            }
        }

        /// The riders an event may ask the core for: Max on a core with the big field, Original before it.
        public static int Clamp(int riders) => Math.Max(1, Math.Min(riders, CoreMax));

        /// Worker, after rx_session_create and before a career round loads: the core's FIELD SIZE opt-in (rx_session_set_field_size: the
        /// game's MaxRidersEventType / MaxRidersVehicleClass tables answer `riders`). An exhibition needs none (spec.riders is taken as is).
        /// Nothing for a field of Original or fewer (the original's path: the setting stays 0). False with why when the core refused it or
        /// has no such switch.
        public static bool SetField(IntPtr session, int riders, out string why)
        {
            why = null;
            if (!Big(riders)) return true;
            try
            {
                int rc = RxApiNative.rx_session_set_field_size(session, Math.Min(riders, Max));
                if (rc != 0) { why = "the core refused a field of " + riders + " riders (" + rc + ")"; return false; }
                return true;
            }
            catch (EntryPointNotFoundException) { why = "this librxcore has no FIELD SIZE (rx_session_set_field_size)"; return false; }
        }
    }
}

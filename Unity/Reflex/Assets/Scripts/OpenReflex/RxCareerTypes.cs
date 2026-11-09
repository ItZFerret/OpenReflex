using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    public enum RxCareerResult : int
    {
        Ok = 0, Repeat = 1, Invalid = 2, Inactive = 3, Full = 4, Stale = 5,
        WrongKind = 6, NotDelivered = 7, Conflict = 8, Busy = 9, Exhausted = 10,
        NotFound = 11, Empty = 12, OutOfOrder = 13, Unsupported = 100,
        BadState = 101, Fault = 102, Locked = 103
    }
    [Flags]
    public enum RxCareerPresenter : uint
    {
        NationalHud = 1, Hub = 2, Rewards = 4, Career = 8,
        Fade = 16, Wipe = 32, Results = 64, Rules = 128, WaypointHud = 256, SupercrossHud = 512, PowercrossHud = 1024,
        // Version 2 (core/include/rx_career_series.h): the HUD route of the other activities and the format HUD elements.
        TerracrossHud = 2048, FreestyleHud = 4096, SectionRaceHud = 8192, OutrunHud = 16384, Eliminator = 32768, Overtake = 65536,
        AllV2 = 131071,
        // The Heat format's pages (rx_career_heat.h): the round results, the gate page of every round and the AAA loading page between them.
        Heat = 131072, AllV3 = 262143
    }
    public enum RxCareerRequestKind : uint
    { UiEvent = 1, UiRoute = 2, Hub = 3, FadeOut = 4, PageWipe = 5, Checkpoint = 6 }
    public enum RxCareerPhase : uint
    { None = 0, Configured = 1, Loaded = 2, VehicleSelected = 3, Initialized = 4, Started = 5, Aborted = 6 }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxCareerConfig
    {
        public uint size, version, controller, presenter_capabilities;
        public static RxCareerConfig Create(RxCareerPresenter capabilities) => new RxCareerConfig
        { size = (uint)Marshal.SizeOf<RxCareerConfig>(), version = 1, presenter_capabilities = (uint)capabilities };
    }
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerSeriesInfo
    {
        public uint size, version, series_id, authored, supported, locked, eligible;
        public int product_id, blocking_product_id, vehicle_class, event_type_id;
        public uint unowned_reward_vehicle_count;
        public fixed byte owner[32], format[32], activity[32];
        public static RxCareerSeriesInfo Create() => new RxCareerSeriesInfo { size = (uint)sizeof(RxCareerSeriesInfo), version = 1 };
        public string Owner { get { fixed (byte* p = owner) return RxFixed.GetUtf8(p, 32); } }
        public string Format { get { fixed (byte* p = format) return RxFixed.GetUtf8(p, 32); } }
        public string Activity { get { fixed (byte* p = activity) return RxFixed.GetUtf8(p, 32); } }
    }
    // rx_career_series_presentation (core/include/rx_career_series.h, version 1): what a series needs from the presenter.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerSeriesPresentation
    {
        public uint size, version, series_id, authored, admitted, format, event_vtable, activity_type, activity_mode, required_capabilities;
        public int duration, riders, locations, point_id;
        public fixed byte hud_route[64], ai_format[16], reason[128];
        public static RxCareerSeriesPresentation Create() => new RxCareerSeriesPresentation { size = (uint)sizeof(RxCareerSeriesPresentation), version = 1 };
        public string HudRoute { get { fixed (byte* p = hud_route) return RxFixed.GetUtf8(p, 64); } }
        public string AiFormat { get { fixed (byte* p = ai_format) return RxFixed.GetUtf8(p, 16); } }
        public string Reason { get { fixed (byte* p = reason) return RxFixed.GetUtf8(p, 128); } }
    }
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerRequest
    {
        public uint size, version;
        public ulong generation, id;
        public uint kind, state, delivered, wipe_milestone;
        public float seconds;
        public uint color, controller, reserved;
        public fixed byte name[256];
        public static RxCareerRequest Create() => new RxCareerRequest { size = (uint)sizeof(RxCareerRequest), version = 1 };
        public string Name { get { fixed (byte* p = name) return RxFixed.GetUtf8(p, 256); } }
    }
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerResultRow
    {
        public int rank, player_type, model_id;
        public uint points, gold, silver, bronze, holeshot, dnf;
        public fixed byte name[128];
        public string Name { get { fixed (byte* p = name) return RxFixed.GetUtf8(p, 128); } }
    }
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerStatus
    {
        public uint size, version;
        public ulong generation;
        public uint phase, controller, series_id, session_state, pending_requests;
        public uint manager_active, event_present, event_phase, event_next_phase;
        public uint post_event_complete, activity_present, activity_state, activity_substate;
        public uint continue_allowed, continue_submitted, checkpoint_error, records_finalized;
        public int selected_vehicle_class, local_rank;
        public uint results_count, reward_valid, reward_base, reward_bonus;
        public uint reward_credits_flag, reward_purchase_flag, reward_unlock_flag;
        public fixed byte event_status[32], location[64];
        // Inline C array of sixteen 164-byte rows, with no managed references.
        public fixed byte results[16 * 164];
        public static RxCareerStatus Create() => new RxCareerStatus { size = (uint)sizeof(RxCareerStatus), version = 1 };
        public string EventStatus { get { fixed (byte* p = event_status) return RxFixed.GetUtf8(p, 32); } }
        public string Location { get { fixed (byte* p = location) return RxFixed.GetUtf8(p, 64); } }
        public RxCareerResultRow Result(int index)
        {
            if (results_count > 16 || index < 0 || (uint)index >= results_count) throw new ArgumentOutOfRangeException(nameof(index));
            fixed (byte* p = results) return ((RxCareerResultRow*)p)[index];
        }
    }
}

using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    public enum RxCareerRulesState : uint
    { None = 0, Requested = 1, Shown = 2, Wiping = 3, HideRequested = 4, Ended = 5, Finished = 6, Rejected = 7 }
    public enum RxCareerLoadingState : uint
    { Inactive = 0, Active = 1, Wiping = 2, RouteHub = 3, HubEntered = 4, Finished = 5, Rejected = 6 }

    public static class RxCareerTransitions
    {
        // An explicit presenter contract, not a default or a declaration that a UI exists.
        public const RxCareerPresenter RequiredPresenters = (RxCareerPresenter)243;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerTransitionStatus
    {
        public const uint ByteSize = 184;
        public uint size, version;
        public ulong generation, rules_request_id, rules_wipe_id, loading_wipe_id;
        public uint rules_state, do_not_show_chosen, loading_state, loading_entry_supported;
        public fixed byte rules_type[64], required_ui_event[64];
        public static RxCareerTransitionStatus Create() => new RxCareerTransitionStatus { size = ByteSize, version = 1 };
        public string RulesType { get { fixed (byte* p = rules_type) return RxFixed.GetUtf8(p, 64); } }
        public string RequiredUiEvent { get { fixed (byte* p = required_ui_event) return RxFixed.GetUtf8(p, 64); } }
    }

    // Separate additive adapter keeps checkpoint ownership and legacy controlled
    // adapters unchanged. A Rules-capable launch must supply these operations.
    internal interface ICareerTransitionWorkerNative
    {
        RxCareerResult Transitions(IntPtr session, ulong generation, ref RxCareerTransitionStatus status);
        RxCareerResult RulesAccept(IntPtr session, ulong generation, ulong requestId, bool doNotShowAgain);
        RxCareerResult RulesPopupEnded(IntPtr session, ulong generation, ulong wipeId);
        RxCareerResult LoadingEnter(IntPtr session, ulong generation);
        RxCareerResult LoadingHubEntered(IntPtr session, ulong generation, ulong wipeId);
    }

    public static class RxCareerTransitionsNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StatusFn(IntPtr s, ulong generation, ref RxCareerTransitionStatus status);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ChoiceFn(IntPtr s, ulong generation, ulong id, uint choice);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int IdFn(IntPtr s, ulong generation, ulong id);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ActionFn(IntPtr s, ulong generation);
        static readonly RxNativeLoader.Fn<StatusFn> status = new RxNativeLoader.Fn<StatusFn>("rx_session_career_transitions");
        static readonly RxNativeLoader.Fn<ChoiceFn> accept = new RxNativeLoader.Fn<ChoiceFn>("rx_session_career_rules_accept");
        static readonly RxNativeLoader.Fn<IdFn> ended = new RxNativeLoader.Fn<IdFn>("rx_session_career_rules_popup_ended");
        static readonly RxNativeLoader.Fn<ActionFn> enter = new RxNativeLoader.Fn<ActionFn>("rx_session_career_loading_enter");
        static readonly RxNativeLoader.Fn<IdFn> hub = new RxNativeLoader.Fn<IdFn>("rx_session_career_loading_hub_entered");
        // Simulation owner only. Missing exports throw; there is no acknowledgement fallback.
        public static RxCareerResult Status(IntPtr s, ulong g, ref RxCareerTransitionStatus value) => (RxCareerResult)status.Get()(s, g, ref value);
        public static RxCareerResult RulesAccept(IntPtr s, ulong g, ulong id, bool doNotShowAgain) => (RxCareerResult)accept.Get()(s, g, id, doNotShowAgain ? 1u : 0u);
        public static RxCareerResult RulesPopupEnded(IntPtr s, ulong g, ulong id) => (RxCareerResult)ended.Get()(s, g, id);
        // Current native owner deliberately returns Unsupported for complete AAA entry.
        public static RxCareerResult LoadingEnter(IntPtr s, ulong g) => (RxCareerResult)enter.Get()(s, g);
        public static RxCareerResult LoadingHubEntered(IntPtr s, ulong g, ulong id) => (RxCareerResult)hub.Get()(s, g, id);
    }
}

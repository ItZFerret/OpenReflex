using System;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // An installation reports only capabilities backed by its live renderers and route providers.
    // No default implementation or menu entry is installed by the runtime candidate.
    public interface IOwnCareerPresentationInstallation
    {
        RxCareerPresenter Capabilities { get; }
        IOwnCareerRuntimePresenter Create(OwnCareerRuntimeContext context);
    }
    public interface IOwnCareerRuntimePresenter : IDisposable
    {
        bool BlocksInput { get; }
        void Tick(object world, ulong generation, double clock);
        // Evidence of the actual current chase-camera render after a native pose was applied.
        // The adapter may release only a matching pending world handoff, never other route acknowledgements.
        void WorldRendered(object world, ulong generation);
    }
    public sealed class OwnCareerRuntimeContext
    {
        public Transform Parent { get; }
        public object WorldIdentity { get; }
        public ulong Generation { get; }
        public CareerWorker Worker { get; }
        public Func<bool> IsCurrent { get; }
        public Action WakeCommands { get; }
        // Fatal installation failure: schedules session teardown. Do not use for retryable checkpoint notices.
        public Action<string> Fail { get; }
        /// The world's Gate Selection view (the 3D camera and cards of the gate page); null when the world has none.
        public OwnGateView Gate { get; }
        /// The world's end-of-race replay renderer is installed (PostEventMenu's EventHasReplay: the results page offers HIGHLIGHT REEL). Read when the
        /// page is built: the renderer comes up after the presenter.
        public Func<bool> ReplayAvailable { get; }
        internal OwnCareerRuntimeContext(Transform parent, object identity, ulong generation, CareerWorker worker,
            Func<bool> current, Action wake, Action<string> fail, OwnGateView gate = null, Func<bool> replayAvailable = null)
        { Parent=parent; WorldIdentity=identity; Generation=generation; Worker=worker; IsCurrent=current; WakeCommands=wake; Fail=fail; Gate=gate; ReplayAvailable=replayAvailable ?? (() => false); }
    }
}

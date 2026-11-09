// TEST ONLY observational helper. Does not call native APIs, dispatch receipts or mutate renderers.
using System;
using System.Reflection;
using System.Threading.Tasks;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

public sealed class ReplayFlowDiagnostics
{
    const BindingFlags Hidden = BindingFlags.Instance | BindingFlags.NonPublic;
    static readonly FieldInfo Renderer = typeof(OriginalWorldRuntime).GetField("replayRenderer", Hidden);
    string lastKey; double next; ulong lastFrame = ulong.MaxValue;
    public bool PaintedPendingDrained { get; private set; }
    public bool SawActive { get; private set; }
    public bool SawPending { get; private set; }
    public bool SawValidCamera { get; private set; }
    public bool SawDrain { get; private set; }
    public uint DistinctActiveFrames { get; private set; }
    static object Read(object owner, string name)
    { var field = owner.GetType().GetField(name, Hidden); if (field == null) throw new MissingFieldException(owner.GetType().FullName, name); return field.GetValue(owner); }
    public void Sample(OriginalWorldRuntime runtime, OpenReflexSimulation simulation)
    {
        var snapshot = simulation?.Replay?.Snapshot;
        if (snapshot == null) return;
        var view = snapshot.View;
        SawActive |= view.active != 0; SawValidCamera |= view.active != 0 && view.camera_valid != 0; SawPending |= snapshot.Count != 0 || view.pending_requests != 0;
        if (view.active != 0 && view.frame != lastFrame) { DistinctActiveFrames++; lastFrame = view.frame; }
        object renderer = runtime != null ? Renderer?.GetValue(runtime) : null;
        ulong applied = 0, acknowledged = 0, epoch = 0, painted = 0, pose = 0, cameraFrame = 0;
        Task receipt = null; bool exact = false, disposed = false;
        if (renderer != null)
        {
            applied = (ulong)Read(renderer, "lastAppliedId"); acknowledged = (ulong)Read(renderer, "lastAcknowledgedId");
            epoch = (ulong)Read(renderer, "epoch"); painted = (ulong)Read(renderer, "paintedEpoch");
            pose = (ulong)Read(renderer, "poseFrame"); cameraFrame = (ulong)Read(renderer, "cameraNativeFrame");
            exact = (bool)Read(renderer, "exactPose"); disposed = (bool)Read(renderer, "disposed"); receipt = Read(renderer, "receipt") as Task;
            // Observational drain only: native queue empty AND current renderer receipts consumed.
            // This never sends ACK or substitutes for actual render callbacks.
            if ((SawActive || SawPending) && !disposed && view.active == 0 && view.pending_requests == 0 && snapshot.Count == 0 && receipt == null && acknowledged >= applied && (!snapshot.DrawFade || epoch == painted)) SawDrain = true;
        }
        PaintedPendingDrained=renderer!=null&&!disposed&&view.pending_requests==0&&snapshot.Count==0&&receipt==null&&acknowledged>=applied&&(!snapshot.DrawFade||epoch==painted);
        string head = snapshot.Count != 0 ? snapshot.Request(0).id + "/" + snapshot.Request(0).operation : "none";
        string key = view.generation + "/" + view.active + "/" + view.camera_valid + "/" + view.pending_requests + "/" + snapshot.Count + "/" + head + "/" + (receipt?.Status.ToString() ?? "none") + "/" + SawDrain;
        if (key == lastKey && Time.realtimeSinceStartupAsDouble < next) return;
        lastKey = key; next = Time.realtimeSinceStartupAsDouble + 3;
        Debug.Log("[career-replay-probe] generation=" + view.generation + " nativeFrame=" + view.frame + " active=" + view.active + " cameraValid=" + view.camera_valid + " nativePending=" + view.pending_requests + " copiedPending=" + snapshot.Count + " head=" + head + " fade=" + snapshot.DrawFade + " renderer=" + (renderer != null) + " disposed=" + disposed + " applied=" + applied + " acknowledged=" + acknowledged + " receipt=" + (receipt?.Status.ToString() ?? "none") + " paintEpoch=" + painted + "/" + epoch + " poseFrame=" + pose + " cameraFrame=" + cameraFrame + " exactPose=" + exact + " observedDrain=" + SawDrain + " activeFrames=" + DistinctActiveFrames);
    }
}

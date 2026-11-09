using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.RxCore;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;
namespace Reflex.Port.OriginalWorld
{
    // Main-thread projection of actual native replay camera and original fade state.
    // Layer 5 is after the 3D camera and below the owned results/curtain UI (75/100).
    public sealed class OwnReplayRenderer : IDisposable
    {
        sealed class FadeLayer:VisualElement
        {
            public Color Color;public ulong Epoch;public Action<ulong> Painted;
            public FadeLayer(){pickingMode=PickingMode.Ignore;style.position=Position.Absolute;style.left=0;style.right=0;style.top=0;style.bottom=0;generateVisualContent+=Draw;}
            void Draw(MeshGenerationContext c)
            {
                if(panel==null||layout.width<=0||layout.height<=0)return;
                var p=c.painter2D;p.fillColor=Color;p.BeginPath();p.MoveTo(Vector2.zero);p.LineTo(new Vector2(layout.width,0));p.LineTo(new Vector2(layout.width,layout.height));p.LineTo(new Vector2(0,layout.height));p.ClosePath();p.Fill();Painted?.Invoke(Epoch);
            }
        }
        readonly int owner=Thread.CurrentThread.ManagedThreadId;readonly Camera camera;readonly object world;readonly ulong generation;
        readonly ReplayWorker worker;readonly Func<bool> current;readonly Action wake;readonly GameObject host;readonly VisualElement root;readonly FadeLayer fade;
        readonly ReplayCameraLensLease lensLease=new ReplayCameraLensLease();
        readonly Rect priorRect;ReplaySnapshot applied;ulong epoch,paintedEpoch,lastAppliedId,lastAcknowledgedId;int cameraFrame=-1;ulong cameraNativeFrame;
        ulong poseFrame; bool exactPose;
        Task<RxCareerResult[]> receipt;ulong receiptLastId;bool disposed,viewportOwned;
        public OwnReplayRenderer(Transform parent,Camera camera,object world,ulong generation,ReplayWorker worker,Func<bool> current,Action wake)
        {
            this.camera=camera?camera:throw new ArgumentNullException(nameof(camera));this.world=world??throw new ArgumentNullException(nameof(world));if(generation==0)throw new ArgumentException("Missing replay generation.");
            this.generation=generation;this.worker=worker??throw new ArgumentNullException(nameof(worker));this.current=current??throw new ArgumentNullException(nameof(current));this.wake=wake??throw new ArgumentNullException(nameof(wake));
            priorRect=camera.rect;host=new GameObject("Open Reflex replay fade");host.transform.SetParent(parent,false);
            try{root=OuUi.CreateDocument(host,5);root.pickingMode=PickingMode.Ignore;fade=new FadeLayer();fade.Painted=e=>{if(!disposed&&current())paintedEpoch=e;};}
            catch{UnityEngine.Object.Destroy(host);throw;}
        }
        void Owner(){if(Thread.CurrentThread.ManagedThreadId!=owner)throw new InvalidOperationException("Replay renderer belongs to the Unity main thread.");}
        void PollReceipt()
        {
            if(receipt==null||!receipt.IsCompleted)return;
            var done=receipt;receipt=null;
            if(done.IsCanceled)throw new InvalidOperationException("Replay receipt was cancelled.");
            if(done.IsFaulted)throw done.Exception.GetBaseException();
            foreach(var r in done.Result)if(r!=RxCareerResult.Ok&&r!=RxCareerResult.Repeat)throw new InvalidOperationException("Replay paint was rejected: "+r);
            lastAcknowledgedId=receiptLastId;
        }
        public bool RequiresExactPose => worker.Snapshot is ReplaySnapshot v && (v.View.active!=0 || v.Count!=0 || v.DrawFade);
        public void Tick()
        { Owner();if(disposed)return;if(!current()){Dispose();return;}PollReceipt();if(worker.Snapshot==null){applied=null;fade.RemoveFromHierarchy();} }
        // The runtime calls this only AFTER its matching collected scene poses were applied.
        public void Present(ReplaySnapshot presented,ulong nativeFrame,bool exact)
        {
            Owner();if(disposed||!current())return;poseFrame=nativeFrame;exactPose=exact;
            var view=ReplayPresentedFrame.Select(presented,worker.Snapshot,nativeFrame,exact);
            if(view==null){applied=null;fade.RemoveFromHierarchy();return;}
            if(view.View.generation!=generation)throw new InvalidOperationException("Replay snapshot generation changed.");
            ulong end=view.Count==0?lastAppliedId:view.Request(view.Count-1).id;
            bool change=applied==null||view.View.frame!=applied.View.frame||end>lastAppliedId;
            if(change)
            {
                for(int i=0;i<view.Count;i++)
                {
                    var request=view.Request(i);if(request.id<=lastAppliedId)continue;
                    if(request.generation!=generation||request.frame!=view.View.frame)throw new InvalidOperationException("Replay batch does not match the current native frame.");
                    if(request.operation==1)
                    {if(request.viewport_count!=1)throw new NotSupportedException("Replay split view is not installed.");camera.rect=new Rect(0,0,1,1);viewportOwned=true;}
                    else if(request.operation!=2)throw new NotSupportedException("Unknown replay presentation request.");
                    lastAppliedId=request.id;
                }
                applied=view;epoch++;cameraFrame=-1;
                if(view.DrawFade)
                {
                    // Original FadeToColorVS saturates color. Original blend is SRCALPHA/INVSRCALPHA.
                    fade.Color=new Color(Mathf.Clamp01(view.Red),Mathf.Clamp01(view.Green),Mathf.Clamp01(view.Blue),Mathf.Clamp01(view.Alpha));fade.Epoch=epoch;
                    if(fade.parent==null)root.Add(fade);fade.MarkDirtyRepaint();
                }
                else fade.RemoveFromHierarchy();
            }
            else applied=view; // same-frame receipts may remove queue heads without changing the drawn state.
        }
        static bool Finite(Vector3 value)=>ReplayFadeState.Finite(value.x)&&ReplayFadeState.Finite(value.y)&&ReplayFadeState.Finite(value.z);
        public unsafe bool ApplyCamera()
        {
            Owner();if(disposed)return false;
            if(!current()||applied==null||applied.View.active==0){RestoreLens();return false;}
            if(applied.View.camera_valid==0)throw new InvalidOperationException("Native replay camera is unavailable.");
            var c=applied.View.camera;var forward=new Vector3(c.forward[0],c.forward[1],c.forward[2]);var up=new Vector3(c.up[0],c.up[1],c.up[2]);var position=new Vector3(c.position[0],c.position[1],c.position[2]);
            if(!Finite(position)||!Finite(forward)||!Finite(up)||!ReplayFadeState.Finite(forward.sqrMagnitude)||!ReplayFadeState.Finite(up.sqrMagnitude)||forward.sqrMagnitude<.5f||up.sqrMagnitude<.5f||Vector3.Cross(forward,up).sqrMagnitude<.0001f||!(c.fov>.05f&&c.fov<3f)||!ReplayFadeState.Finite(c.far_plane)||!(c.near_plane>0&&c.far_plane>c.near_plane))throw new InvalidOperationException("Invalid native replay camera.");
            lensLease.Acquire(camera,new ReplayCameraLens(camera.fieldOfView,camera.nearClipPlane,camera.farClipPlane));
            camera.transform.SetPositionAndRotation(position,Quaternion.LookRotation(forward,up));camera.fieldOfView=c.fov*Mathf.Rad2Deg;camera.nearClipPlane=c.near_plane;camera.farClipPlane=c.far_plane;
            lensLease.Applied(camera,new ReplayCameraLens(camera.fieldOfView,camera.nearClipPlane,camera.farClipPlane));
            cameraFrame=Time.frameCount;cameraNativeFrame=applied.View.frame;return true;
        }
        void RestoreLens()
        {
            // Release once even if Unity has already destroyed this same target.
            // Ordinary camera mode code runs afterwards and may set its own lens.
            var currentLens=camera?new ReplayCameraLens(camera.fieldOfView,camera.nearClipPlane,camera.farClipPlane):default;
            if(lensLease.Release(camera,currentLens,out var saved)&&camera)
            {camera.fieldOfView=saved.FieldOfView;camera.nearClipPlane=saved.Near;camera.farClipPlane=saved.Far;}
        }
        public void WorldRendered(List<Camera> cameras)
        {
            Owner();if(disposed||!current()||applied==null||receipt!=null||applied.Count==0)return;
            if(!cameras.Contains(camera)||!exactPose||poseFrame!=applied.View.frame)return;
            if(applied.View.active!=0&&(cameraFrame!=Time.frameCount||cameraNativeFrame!=applied.View.frame))return;
            if(viewportOwned&&camera.rect!=new Rect(0,0,1,1))throw new InvalidOperationException("Replay viewport was replaced before rendering.");
            if(applied.DrawFade&&(fade.parent==null||paintedEpoch!=epoch))return;
            var tasks=new List<Task<RxCareerResult>>();ulong last=lastAcknowledgedId;
            for(int i=0;i<applied.Count;i++)
            {
                var request=applied.Request(i);if(request.id<=lastAcknowledgedId)continue;
                // All requests were applied in original order before this actual paint.
                // Native still consumes each head's Applied then Painted, without a simulation step between them.
                tasks.Add(worker.Acknowledge(request,1));tasks.Add(worker.Acknowledge(request,2));last=request.id;
            }
            if(tasks.Count==0)return;receiptLastId=last;receipt=Task.WhenAll(tasks);wake();
        }
        public void Dispose()
        {
            Owner();if(disposed)return;disposed=true;receipt=null;applied=null;fade.Painted=null;fade.RemoveFromHierarchy();
            RestoreLens();
            if(camera&&viewportOwned)camera.rect=priorRect;UnityEngine.Object.Destroy(host);
        }
    }
}

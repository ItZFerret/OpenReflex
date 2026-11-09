using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
namespace Reflex.Port.RxCore
{
    public sealed class ReplaySnapshot
    {
        readonly RxReplayRequest[] requests;
        public RxReplayView View { get; }
        public int Count=>requests.Length;
        public RxReplayRequest Request(int i)=>requests[i];
        public bool DrawFade { get; }
        public float Red { get; } public float Green { get; } public float Blue { get; } public float Alpha { get; }
        internal ReplaySnapshot(RxReplayView view,List<RxReplayRequest> requests,bool draw,ReplayFadeState fade)
        { View=view;this.requests=requests.ToArray();DrawFade=draw;Red=fade.Red;Green=fade.Green;Blue=fade.Blue;Alpha=fade.Alpha; }
    }
    // Queued receipts and native reads are serialized on OpenReflexSimulation's owner.
    public sealed class ReplayWorker
    {
        sealed class Receipt { public RxReplayRequest request;public uint stage;public readonly TaskCompletionSource<RxCareerResult> done=new TaskCompletionSource<RxCareerResult>(TaskCreationOptions.RunContinuationsAsynchronously); }
        readonly object gate=new object();readonly Queue<Receipt> receipts=new Queue<Receipt>();readonly ReplayFadeState fade=new ReplayFadeState();
        int owner;ulong generation,lastRequest,lastFrame;bool closed,draw,haveFrame,exhibition,ended;ReplaySnapshot snapshot;ulong published;
        public ReplaySnapshot Snapshot { get { lock(gate)return snapshot; } }
        /// The presenter generation once bound (0 before): an exhibition renderer is created from it.
        public ulong Generation { get { lock(gate)return published; } }
        public bool Pending=>Snapshot?.Count>0;
        static void Require(RxCareerResult r,string op){if(r!=RxCareerResult.Ok)throw new InvalidOperationException("Replay "+op+" failed: "+r);}
        void Owner(){if(Thread.CurrentThread.ManagedThreadId!=owner)throw new InvalidOperationException("Replay belongs to the native simulation owner.");}
        internal void BeginWorker(IntPtr session,ulong g)
        { if(g==0||owner!=0||closed)throw new InvalidOperationException("Replay provider cannot bind twice.");owner=Thread.CurrentThread.ManagedThreadId;generation=g;Require(RxReplayNative.Begin(session,g),"binding");lock(gate)published=g;RefreshWorker(session); }
        // Exhibition events: the original plays the end-of-race replay for every race that reaches Post, not only in a career.
        // Bound before rx_session_event_load (before the first frame); the generation is the presenter's own.
        internal void BeginEventWorker(IntPtr session)
        {
            if(owner!=0||closed)throw new InvalidOperationException("Replay provider cannot bind twice.");
            owner=Thread.CurrentThread.ManagedThreadId;
            Require(RxReplayNative.BeginEvent(session,out ulong g),"exhibition binding");
            if(g==0)throw new InvalidOperationException("Replay exhibition binding returned no generation.");
            generation=g;exhibition=true;lock(gate)published=g;RefreshWorker(session);
        }
        // Before the session's final records: the host goes away, its unpainted requests with it (no frame runs afterwards).
        internal void EndEventWorker(IntPtr session)
        {
            if(!exhibition||ended||owner==0)return;Owner();ended=true;
            var r=RxReplayNative.EndEvent(session,generation);
            if(r!=RxCareerResult.Ok&&r!=RxCareerResult.Repeat)throw new InvalidOperationException("Replay exhibition release failed: "+r);
        }
        public Task<RxCareerResult> Acknowledge(RxReplayRequest request,uint stage)
        {
            lock(gate){if(closed||owner==0)return Task.FromResult(RxCareerResult.Inactive);if(request.generation!=generation)return Task.FromResult(RxCareerResult.Stale);
                if(stage!=1&&stage!=2)return Task.FromResult(RxCareerResult.Invalid);if(receipts.Count>=64)return Task.FromResult(RxCareerResult.Full);
                var r=new Receipt{request=request,stage=stage};receipts.Enqueue(r);return r.done.Task;}
        }
        internal void CommandsWorker(IntPtr session)
        { Owner();while(true){Receipt r;lock(gate){if(closed||receipts.Count==0)break;r=receipts.Dequeue();}try{var copy=r.request;r.done.SetResult(RxReplayNative.Ack(session,generation,ref copy,r.stage));}catch(Exception e){r.done.SetException(e);throw;}} }
        internal void RefreshWorker(IntPtr session)
        {
            Owner();if(closed)return;var v=RxReplayView.Create();Require(RxReplayNative.View(session,generation,ref v),"view");
            if(v.generation!=generation||v.pending_requests>32)throw new InvalidOperationException("Invalid replay view identity/queue.");
            var requests=new List<RxReplayRequest>();bool rearmed=false;
            for(uint i=0;i<v.pending_requests;i++)
            {
                var r=RxReplayRequest.Create();Require(RxReplayNative.Peek(session,generation,i,ref r),"request");
                if(r.generation!=generation||r.id==0||r.frame>v.frame)throw new InvalidOperationException("Invalid replay request identity.");requests.Add(r);
                if(r.id<=lastRequest)continue;
                if(r.operation==1){if(r.viewport_count!=1)throw new NotSupportedException("Only the actual single replay viewport is installed.");}
                else if(r.operation==2)
                {
                    if((r.flags&1)==0)throw new InvalidOperationException("Replay fade has no actual native color snapshot.");
                    if((r.fade_kind==2&&r.color_mask!=0)||(r.fade_kind==3&&r.color_mask!=15))throw new InvalidOperationException("Invalid replay fade color write contract.");
                    fade.Begin(r.fade_kind,RxReplayNative.Float(r.duration_bits),true,RxReplayNative.Float(r.red_bits),RxReplayNative.Float(r.green_bits),RxReplayNative.Float(r.blue_bits));rearmed=true;
                }
                else throw new NotSupportedException("Unknown replay operation.");lastRequest=r.id;
            }
            if(!haveFrame||v.frame!=lastFrame||rearmed)
            { draw=fade.Step(RxReplayNative.Float(v.engine_dt_bits));lastFrame=v.frame;haveFrame=true; }
            lock(gate)snapshot=new ReplaySnapshot(v,requests,draw,fade);
        }
        internal void CloseWorker(){lock(gate){closed=true;snapshot=null;while(receipts.Count>0)receipts.Dequeue().done.TrySetResult(RxCareerResult.Inactive);}}
    }
}

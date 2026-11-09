using System;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.RxCore;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;
namespace Reflex.Port.OpenFrontEnd {
 // Owned by the existing main-thread UI lifecycle. No new MonoBehaviour, resources or native calls.
 public sealed class CareerCurtainPresenter : IDisposable {
  sealed class Commands:ICareerCurtainCommands {
   readonly CareerWorker worker;public Commands(CareerWorker worker){this.worker=worker;}
   public Task<RxCareerResult> FadeComplete(CareerCurtainToken t)=>worker.Acknowledge(t.Generation,t.Id,RxCareerRequestKind.FadeOut,true);
   public Task<RxCareerResult> WipeMilestone(CareerCurtainToken t,uint stage)=>worker.WipeCompleted(t.Generation,t.Id,t.Name,stage);
  }
  sealed class Curtain:VisualElement {
   public float Alpha;public uint Argb;public Action Painted;
   public Curtain(){name="careerCurtain";AddToClassList("mxa-layer");pickingMode=PickingMode.Position;RegisterCallback<PointerDownEvent>(e=>e.StopImmediatePropagation());RegisterCallback<PointerUpEvent>(e=>e.StopImmediatePropagation());RegisterCallback<ClickEvent>(e=>e.StopImmediatePropagation());RegisterCallback<WheelEvent>(e=>e.StopImmediatePropagation());generateVisualContent+=Draw;}
   void Draw(MeshGenerationContext context){
    float w=layout.width,h=layout.height;if(w<=0||h<=0)return;
    var p=context.painter2D;p.fillColor=new Color(((Argb>>16)&255)/255f,((Argb>>8)&255)/255f,(Argb&255)/255f,((Argb>>24)&255)/255f*Alpha);
    p.BeginPath();p.MoveTo(Vector2.zero);p.LineTo(new Vector2(w,0));p.LineTo(new Vector2(w,h));p.LineTo(new Vector2(0,h));p.ClosePath();p.Fill();Painted?.Invoke();
   }
  }
  readonly int owner=Thread.CurrentThread.ManagedThreadId;readonly object world;readonly ulong generation;readonly VisualElement root;readonly Curtain curtain;
  readonly CareerCurtainTimeline timeline;VisualElement errorLayer;bool disposed;
  public CareerCurtainTimeline.Stage Phase=>timeline.Phase;
  public CareerCurtainToken Token=>timeline.Token;
  public string Error=>timeline.Error;
  public bool BlocksInput=>!disposed&&Phase!=CareerCurtainTimeline.Stage.Idle&&Phase!=CareerCurtainTimeline.Stage.Done&&Phase!=CareerCurtainTimeline.Stage.Disposed;
  // covered fires only after the native worker accepted the rendered milestone.
  // Host must explicitly finish its real route/Rules work then BeginOut(token).
  public CareerCurtainPresenter(VisualElement overlayRoot,object world,ulong generation,CareerWorker worker,Action<CareerCurtainToken> covered,Action<string> error)
   :this(overlayRoot,world,generation,new Commands(worker??throw new ArgumentNullException(nameof(worker))),covered,error){}
  // Explicit test injection only; production constructor always queues the real worker.
  internal CareerCurtainPresenter(VisualElement overlayRoot,object world,ulong generation,ICareerCurtainCommands commands,Action<CareerCurtainToken> covered,Action<string> error){
   root=overlayRoot??throw new ArgumentNullException(nameof(overlayRoot));this.world=world??throw new ArgumentNullException(nameof(world));this.generation=generation;
   OuTheme.Apply(root);
   curtain=new Curtain();timeline=new CareerCurtainTimeline(world,generation,commands,name=>CareerWipeContent.Load(),covered,message=>{
    if(errorLayer==null){errorLayer=OuUi.Div("mxa-layer",root);OuUi.Text(message,"mxa-error",errorLayer);}error?.Invoke(message);
   });curtain.Painted=timeline.Painted;
  }
  void Owner(){if(Thread.CurrentThread.ManagedThreadId!=owner)throw new InvalidOperationException("Career curtain belongs to the Unity main thread.");}
  public bool Present(object actualWorld,RxCareerRequest request,double clock){Owner();if(disposed)return false;bool accepted=timeline.Present(actualWorld,request,clock);Render();return accepted;}
  public void Tick(object actualWorld,ulong actualGeneration,double clock){Owner();if(disposed)return;if(!ReferenceEquals(world,actualWorld)||generation!=actualGeneration){Dispose();return;}timeline.Tick(clock);Render();}
  public bool BeginOut(CareerCurtainToken token){Owner();if(disposed)return false;return timeline.BeginOut(token);}
  public bool ReleaseFade(CareerCurtainToken token){Owner();if(disposed)return false;bool result=timeline.ReleaseFade(token);Render();return result;}
  void Render(){
   bool show=timeline.Phase!=CareerCurtainTimeline.Stage.Idle&&timeline.Phase!=CareerCurtainTimeline.Stage.Done&&timeline.Phase!=CareerCurtainTimeline.Stage.Disposed;
   if(show){if(curtain.parent==null)root.Add(curtain);curtain.Alpha=timeline.Alpha;curtain.Argb=timeline.Argb;curtain.MarkDirtyRepaint();if(errorLayer!=null)errorLayer.BringToFront();}
   else curtain.RemoveFromHierarchy();
  }
  public void Dispose(){Owner();if(disposed)return;disposed=true;timeline.Dispose();curtain.Painted=null;curtain.RemoveFromHierarchy();errorLayer?.RemoveFromHierarchy();}
 }
}

using System;
using Reflex.Port.RxCore;
using Reflex.Port.OriginalWorld;
using UnityEngine;
namespace Reflex.Port.OpenFrontEnd {
 public interface IOwnCareerExternalPages {
  RxCareerPresenter Capabilities { get; }
  void Present(OwnCareerPresentationNeed need,OwnCareerPresenter coordinator);
  void ReturnToCareer();
 }
 // Manual launcher installation. The factory creates all concrete pages per session.
 public sealed class OwnCareerPresentationInstallation:IOwnCareerPresentationInstallation {
  readonly RxCareerPresenter hud;readonly Action returnToMenu,cancelToMenu;
  const RxCareerPresenter WaypointHud=(RxCareerPresenter)256;
  // Heat: the rounds' gate / results pages and the AAA loading page between them (OwnCareerPages.ShowHeatHub, OwnAaaLoading.ShowHeld).
  const RxCareerPresenter Owned=RxCareerPresenter.Hub|RxCareerPresenter.Career|RxCareerPresenter.Rules|RxCareerPresenter.Fade|RxCareerPresenter.Wipe|RxCareerPresenter.Results|RxCareerPresenter.Rewards|RxCareerPresenter.Heat;
  // The race HUDs OriginalHud draws from the install's layers (the National, Waypoint, Supercross and Powercross packages and, w-career,
  // the Terracross / Freestyle / Section Race / Outrun packages) and the format elements it drives (the eliminator's riders remaining,
  // the overtake start countdown).
  public const RxCareerPresenter InstalledHuds=RxCareerPresenter.NationalHud|WaypointHud|RxCareerPresenter.SupercrossHud|RxCareerPresenter.PowercrossHud|
   RxCareerPresenter.TerracrossHud|RxCareerPresenter.FreestyleHud|RxCareerPresenter.SectionRaceHud|RxCareerPresenter.OutrunHud|
   RxCareerPresenter.Eliminator|RxCareerPresenter.Overtake;
  public OwnCareerPresentationInstallation(RxCareerPresenter installedHud,Action returnToMenu,Action cancelToMenu){
   if(installedHud==0||(installedHud&~InstalledHuds)!=0)throw new ArgumentException("Declare only actually installed HUD providers.");
   hud=installedHud;this.returnToMenu=returnToMenu??throw new ArgumentNullException(nameof(returnToMenu));this.cancelToMenu=cancelToMenu??throw new ArgumentNullException(nameof(cancelToMenu));
  }
  public RxCareerPresenter Capabilities=>Owned|hud;
  public IOwnCareerRuntimePresenter Create(OwnCareerRuntimeContext context)=>new Adapter(context,returnToMenu,cancelToMenu);
  sealed class Adapter:IOwnCareerRuntimePresenter {
   readonly OwnCareerRuntimeContext context;readonly GameObject overlayHost;readonly OwnCareerPresenter coordinator;readonly OwnCareerPages pages;
   OwnCareerPresentationNeed handoff;int requestedFrame;bool disposed;
   public Adapter(OwnCareerRuntimeContext context,Action returnToMenu,Action cancelToMenu){
    this.context=context??throw new ArgumentNullException(nameof(context));if(!context.IsCurrent())throw new InvalidOperationException("Career runtime changed before presenter installation.");
    overlayHost=new GameObject("Open Reflex career curtain");overlayHost.transform.SetParent(context.Parent,false);
    try{
     pages=new OwnCareerPages(context,returnToMenu,cancelToMenu);var root=OuUi.CreateDocument(overlayHost,100);
     coordinator=new OwnCareerPresenter(context.Parent,root,context.WorldIdentity,context.Generation,context.Worker,context.IsCurrent,context.WakeCommands,
      need=>{if(need.Kind==OwnCareerNeed.FadeHandoff||need.Kind==OwnCareerNeed.SeriesWipeHandoff){handoff=need;requestedFrame=Time.frameCount;}else pages.Present(need,coordinator);},
      message=>Debug.LogError("[OwnCareer] "+message),pages.ReturnToCareer,pages.CancelEvent,context.ReplayAvailable);
     pages.BindCoordinator(context,coordinator);
    }catch{coordinator?.Dispose();pages?.Dispose();UnityEngine.Object.Destroy(overlayHost);throw;}
   }
   public bool BlocksInput=>!disposed&&(coordinator.BlocksInput||pages.BlocksInput);
   public void Tick(object world,ulong generation,double clock){if(disposed)return;if(!context.IsCurrent()){Dispose();return;}pages.Tick();coordinator.Tick(world,generation,clock);}
   public void WorldRendered(object world,ulong generation){
    if(disposed||!context.IsCurrent()||!ReferenceEquals(world,context.WorldIdentity)||generation!=context.Generation||handoff==null||Time.frameCount<requestedFrame)return;
    bool ready=handoff.Kind==OwnCareerNeed.FadeHandoff?coordinator.ReportFadeHandoffReady(handoff.Token):coordinator.ReportSeriesWipeReady(handoff.Token);if(ready)handoff=null;
   }
   public void Dispose(){if(disposed)return;disposed=true;handoff=null;coordinator.Dispose();pages.Dispose();UnityEngine.Object.Destroy(overlayHost);}
  }
 }
}

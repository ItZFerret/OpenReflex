using Reflex.Port.RxCore;
namespace Reflex.Port.OpenFrontEnd {
 public enum OwnCareerNeed { RulesShow, RulesHide, LoadingHub, Hub, FadeHandoff, SeriesWipeHandoff, CareerRoute }
 // Issued only from this coordinator; retain the exact object for readiness reports.
 public sealed class OwnCareerUiToken {
  public readonly object World;public readonly ulong Generation,Id;public readonly RxCareerRequestKind Kind;public readonly string Name;
  internal readonly CareerCurtainToken Curtain;
  internal OwnCareerUiToken(object world,RxCareerRequest request){World=world;Generation=request.generation;Id=request.id;Kind=(RxCareerRequestKind)request.kind;Name=request.Name;}
  internal OwnCareerUiToken(CareerCurtainToken token){World=token.World;Generation=token.Generation;Id=token.Id;Kind=token.Kind;Name=token.Name;Curtain=token;}
 }
 public sealed class OwnCareerPresentationNeed {
  public readonly OwnCareerNeed Kind;public readonly OwnCareerUiToken Token;public readonly string RulesType;
  internal OwnCareerPresentationNeed(OwnCareerNeed kind,OwnCareerUiToken token,string rulesType=null){Kind=kind;Token=token;RulesType=rulesType;}
 }
}

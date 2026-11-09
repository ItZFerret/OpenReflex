using Cursor = UnityEngine.Cursor;
using System;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;
namespace Reflex.Port.OpenFrontEnd {
 // A small owned page renderer; readiness comes from the exact UI traversal epoch.
 internal sealed class OwnCareerPageView:IDisposable {
  readonly GameObject host;readonly VisualElement root,body;ScrollView content;readonly OuNav nav;readonly Func<bool> current;
  OuMenuInput input;VisualElement first,last;bool visible,disposed,armed,committed,cursorLease;
  int shownFrame,hiddenFrame;ulong epoch,drawn,hiddenDrawn;
  public ulong Epoch=>epoch;public bool Visible=>visible&&!disposed&&current();
  bool Geometry(){for(var e=body;e!=null;e=e.parent)if(!e.visible||e.resolvedStyle.display==DisplayStyle.None||e.resolvedStyle.visibility!=Visibility.Visible||e.resolvedStyle.opacity<=0)return false;return body.panel!=null&&body.worldBound.width>0&&body.worldBound.height>0;}
  public bool Rendered(ulong e)=>Visible&&e==epoch&&drawn==e&&Geometry()&&Time.frameCount>shownFrame;
  public bool HiddenRendered(ulong e)=>!visible&&!disposed&&current()&&e==epoch&&hiddenDrawn==e&&root.panel!=null&&Time.frameCount>hiddenFrame;
  public OwnCareerPageView(Transform parent,Func<bool> current){
   this.current=current??throw new ArgumentNullException(nameof(current));host=new GameObject("Open Reflex career pages");host.transform.SetParent(parent,false);
   try{
    OuUi.EnsureEventSystem();root=OuUi.CreateDocument(host,80);OuTheme.Apply(root);var style=Resources.Load<StyleSheet>(OuUi.ResourceRoot+"OuRaceResults");if(!style)throw new InvalidOperationException("Missing owned career page style.");root.styleSheets.Add(style);
    body=OuUi.Div("mxa-layer mxa-dim ou-race-results oc-over");nav=new OuNav{Screen=body,OnCancel=()=>true};
    body.generateVisualContent+=c=>{if(Visible&&Geometry()&&Time.frameCount>shownFrame)drawn=epoch;};body.RegisterCallback<DetachFromPanelEvent>(e=>drawn=0);
    root.generateVisualContent+=c=>{if(!visible&&body.parent==null&&root.panel!=null&&root.worldBound.width>0&&root.worldBound.height>0&&Time.frameCount>hiddenFrame)hiddenDrawn=epoch;};
   }catch{UnityEngine.Object.Destroy(host);throw;}
  }
  /// The Gate Selection page (gates=true): the world is the page. Its camera is on the focused gate (OwnGateView), so no veil and no side column:
  /// the title top left, the gates in a strip above the legend; GateFocus tells which gate the focus is on, GateEnded that the page went.
  public Action<int> GateFocus;public Action GateEnded;bool gateMode;
  /// The hand-off between the gate pick and the race (w-uibatch: "cut the preparing race screen"): the page is up for its owner (its render is
  /// reported, BlocksInput holds the race's input until the release at Pre) but draws nothing: no veil, title, text or legend, so the world (the
  /// race's own intro, the start board) is what the player sees. Blank = this page is shown.
  public bool Blank=>blank&&Visible;bool blank;
  public void ShowBlank(){Show(null,null,false,true);}
  public void Show(string title,string subtitle,bool gates=false){Show(title,subtitle,gates,false);}
  void Show(string title,string subtitle,bool gates,bool blankPage){
   if(disposed||!current())throw new InvalidOperationException("Career page owner changed.");if(epoch==ulong.MaxValue)throw new InvalidOperationException("Career page epoch exhausted.");epoch++;drawn=hiddenDrawn=0;
   if(gateMode&&!gates)EndGateMode();gateMode=gates;
   if(!cursorLease){cursorLease=true;Reflex.Port.Platform.ReflexCursor.Want(this, true);}
   visible=true;armed=committed=false;shownFrame=Time.frameCount;input?.Dispose();input=new OuMenuInput();body.Clear();first=last=null;gateGrid=null;lastGate=false;rows.Clear();if(body.parent==null)root.Add(body);
   body.EnableInClassList("oc-gate-view",gates);blank=blankPage;body.EnableInClassList("oc-blank",blankPage);
   if(blankPage){page=null;side=null;actions=null;content=null;body.MarkDirtyRepaint();return;}
   if(gates){
    OcUi.Header(body,title,subtitle);page=null;side=null;
    actions=OuUi.Div("oc-gate-actions",body);
    var bar=OuUi.Div("oc-gate-bar",body);
    content=new ScrollView(ScrollViewMode.Vertical){name="career-page-content"};content.AddToClassList("oc-gate-scroll");content.style.flexShrink=1;content.style.minHeight=0;
    content.horizontalScrollerVisibility=ScrollerVisibility.Hidden;content.verticalScrollerVisibility=ScrollerVisibility.Hidden;bar.Add(content);body.MarkDirtyRepaint();
    OcUi.Legend(body,(OuUi.KeySubmit,"SELECT",null));
    OuMotion.Enter(bar,0.05f,0.02f,true);
    return;
   }
   OcUi.Stage(body,false);OcUi.Header(body,title,subtitle);
   // The page: a side column (the hero and the actions: always on screen, so CONTINUE is never below the fold on a 720p window) and
   // the content column, which scrolls (a results page of 12 rows + headers is taller than a 720p window).
   page=OuUi.Div("oc-page",body);side=OuUi.Div("oc-page-side",page);actions=OuUi.Div("oc-actions",side);
   var column=OuUi.Div("oc-page-main",page);
   content=new ScrollView(ScrollViewMode.Vertical){name="career-page-content"};content.AddToClassList("oc-page-scroll");content.style.flexGrow=1;content.style.flexShrink=1;content.style.minHeight=0;
   content.horizontalScrollerVisibility=ScrollerVisibility.Hidden;column.Add(content);body.MarkDirtyRepaint();
   OcUi.Legend(body,(OuUi.KeySubmit,"SELECT",null));
   OuMotion.Enter(side,0.05f,0.02f,true);OuMotion.Enter(content.contentContainer,0.04f,0.1f);
  }
  void EndGateMode(){gateMode=false;GateEnded?.Invoke();}
  readonly System.Collections.Generic.List<VisualElement> rows=new System.Collections.Generic.List<VisualElement>();
  VisualElement page,side,actions;
  /// The side column's title and line (the series / the event / the round).
  public void Hero(string title,string sub=null){
   if(side==null)return;var panel=OcUi.Panel(null,(title??"").ToUpperInvariant(),out var panelBody,out var panelTitle,"oc-hero-card");side.Insert(0,panel);
   if(!string.IsNullOrEmpty(sub)){var l=OuUi.Text(sub,"oc-fact-value oc-hero-name",panelBody);l.enableRichText=false;}else panelBody.style.display=DisplayStyle.None;
  }
  /// A paragraph of the content column.
  public void Text(string value){if(blank)return;var label=OuUi.Text(value??"","oc-text oc-text-light",content??body);label.enableRichText=false;}
  /// A small heading of the content column.
  public void Heading(string value){if(blank)return;var label=OuUi.Text(value??"","oc-heading",content??body);label.enableRichText=false;}
  /// A board row (a rider and what they have): navigable, so a controller scrolls the field; rank 0 = no rank shown.
  public VisualElement Row(int rank,string name,bool you,string status=null,string points=null,string time=null){
   if(blank)return null;var row=OcUi.BoardRow(content??body,rank,name,you,status,time,points);rows.Add(row);
   row.RegisterCallback<FocusInEvent>(_=>content?.ScrollTo(row));
   return row;
  }
  VisualElement gateGrid;bool lastGate;
  public void Button(string label,string name,Func<bool> action){
   if(blank)return;ulong shown=epoch;bool gate=name!=null&&name.Contains("-gate-");
   VisualElement host=actions??content??body;
   // the start gates are tiles in a grid of the content column (a Moto has many; a column of buttons would run off the page)
   if(gate){if(gateGrid==null||gateGrid.parent==null)gateGrid=OuUi.Div("oc-gates",content??body);host=gateGrid;}
   var b=OcUi.Button(label,name,()=>{if(!Rendered(shown)||!armed||committed)return;if(action())committed=true;},host,gate?"oc-gate":label=="LEAVE EVENT"?"oc-btn-quiet":null);
   b.RegisterCallback<FocusInEvent>(_=>{if(content!=null&&content.contentContainer.Contains(b))content.ScrollTo(b);});   // keyboard / gamepad focus brings the control into view
   if(gate&&GateFocus!=null){int at=name.LastIndexOf("-gate-",StringComparison.Ordinal);if(at>=0&&int.TryParse(name.Substring(at+6),out var gi))b.RegisterCallback<FocusInEvent>(_=>GateFocus?.Invoke(gi));}   // the 3D camera follows the focused gate
   if(first==null)first=b;if(last!=null&&!(gate&&lastGate)){((OuNavItem)last.userData).down=b;((OuNavItem)b.userData).up=last;}last=b;lastGate=gate;nav.DefaultItem=()=>first;
   LinkRows();
   body.schedule.Execute(()=>{if(Visible&&epoch==shown&&first?.panel!=null)nav.Focus(first);});
  }
  // rows and actions: right from a row reaches the first action, left from it the first row
  void LinkRows(){
   if(first==null||rows.Count==0)return;
   var f=(OuNavItem)first.userData;f.left=rows[0];
   foreach(var r in rows)((OuNavItem)r.userData).right=first;
   OuWidgets.LinkColumn(rows);
  }
  static bool Held(){var k=Keyboard.current;if(k!=null&&(k.enterKey.isPressed||k.numpadEnterKey.isPressed||k.spaceKey.isPressed||k.escapeKey.isPressed))return true;if(Mouse.current?.leftButton.isPressed==true)return true;foreach(var p in Gamepad.all)if(p.buttonSouth.isPressed||p.buttonEast.isPressed||p.startButton.isPressed)return true;return false;}
  public void Tick(bool allowInput){
   if(disposed)return;if(!current()){Dispose();return;}
   if(Visible){if(!Rendered(epoch)&&Time.frameCount>shownFrame)body.MarkDirtyRepaint();if(!allowInput||committed||Time.frameCount==shownFrame)return;if(!armed){if(!Held())armed=true;return;}nav.Update(input);}
   else if(hiddenDrawn!=epoch&&Time.frameCount>hiddenFrame)root.MarkDirtyRepaint();
  }
  public void Hide(bool restoreCursor=true){if(disposed||!visible)return;if(gateMode)EndGateMode();visible=false;blank=false;drawn=hiddenDrawn=0;hiddenFrame=Time.frameCount;body.RemoveFromHierarchy();root.MarkDirtyRepaint();input?.Dispose();input=null;if(restoreCursor)RestoreCursor();}
  void RestoreCursor(){if(!cursorLease)return;cursorLease=false;Reflex.Port.Platform.ReflexCursor.Want(this, false);}
  public void Dispose(){if(disposed)return;Hide();disposed=true;input?.Dispose();input=null;RestoreCursor();UnityEngine.Object.Destroy(host);}
 }
}

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        static bool s_returnCareer;
        int _careerFocusSeries=-1;
        public void ShowCoreCareer()
        {
            var presentation = new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, ReturnToOwnedCareer, ReturnToOwnedCareer);
            // MainMenu:CareerMode: the Career Welcome page until the player has accepted it once (PlayerEvent "CareerWelcome")
            if (CareerWelcomeDue()) { ShowCareerWelcome(() => ShowOwnedCareer(presentation)); return; }
            ShowOwnedCareer(presentation);
        }
        public static void ReturnToOwnedCareer()
        {
            s_returnEvent = null; s_returnFreeRide = false;
            s_returnCareer = true;
            try { UnityEngine.SceneManagement.SceneManager.LoadScene("FrontEnd"); }
            catch { s_returnCareer = false; throw; }
        }
        CancellationTokenSource careerCatalogCancel;
        Task<IReadOnlyList<CareerEventChoice>> careerCatalogTask;
        void ReleaseCareerCatalog()
        {
            careerCatalogCancel?.Cancel(); careerCatalogCancel?.Dispose(); careerCatalogCancel=null;
            if(careerCatalogTask!=null) careerCatalogTask.ContinueWith(t=>{var observed=t.Exception;},TaskContinuationOptions.OnlyOnFaulted);
            careerCatalogTask=null;
        }
        /// The career screens are the original's kind of page: the shell's own bottom bars step aside for the page's legend band.
        void CareerChrome() { _hints.style.display = DisplayStyle.None; _status.style.display = DisplayStyle.None; }

        // The core hub and post-race Return use the same owned career entry point.
        public void ShowOwnedCareer(IOwnCareerPresentationInstallation presentation)
        {
            if(presentation==null) throw new ArgumentNullException(nameof(presentation));
            Clear(Screen.Working,"career"); SetBackground(null); MusicTier(2); CareerChrome();
            OcRaceAndRide hub=null;
            int focusSeries=_careerFocusSeries;_careerFocusSeries=-1;
            _careerStarting=false;
            hub=new OcRaceAndRide(_layer,_nav,ShowHome,choice=>ChooseCareerEvent(choice,presentation,hub));
            hub.Locked=()=>ShowLockedPopup();
            hub.CanQuickView=choice=>CareerQuickViewTypes(choice).Count>0 && !choice.Locked;
            hub.QuickView=choice=>ShowCareerVehicleList(choice,()=>{_careerFocusSeries=choice.Entry.SeriesId;ShowOwnedCareer(presentation);});
            _nav.OnCancel=()=>{ShowHome();return true;};_nav.DefaultItem=()=>hub.DefaultFocus;
            try
            {
                if(!RxNativeLoader.IsLoaded)
                {
                    string path=RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(UnityEngine.Application.dataPath,UnityEngine.Application.isEditor));
                    if(path==null) throw new System.IO.FileNotFoundException("The game core is missing from this client. Please reinstall the full client folder.");
                    string temporary=UnityEngine.Application.isEditor ? System.IO.Path.Combine(System.IO.Path.GetDirectoryName(UnityEngine.Application.dataPath),"Temp","OpenReflex") : null;
                    RxNativeLoader.Load(path,temporary);
                }
                var entries=CareerEventCatalog.Read();var persistence=CoreGarageProfile.CapturePersistence();
                bool all=CoreGarageProfile.UnlockEverythingEnabled;
                careerCatalogCancel=new CancellationTokenSource();
                var token=careerCatalogCancel.Token;
                careerCatalogTask=OpenReflexSimulation.ReadCareerCatalogAsync(ReflexInstall.Require(),entries,persistence,presentation.Capabilities,token);
                var query=careerCatalogTask;
                ScreenUpdate=()=>
                {
                    if(token.IsCancellationRequested || query!=careerCatalogTask || !query.IsCompleted) return;
                    ScreenUpdate=null;
                    if(query.IsCanceled) return;
                    if(query.IsFaulted){hub.ShowMessage(query.Exception.GetBaseException().Message);return;}
                    string text=all ? "UNLOCKED PROFILE · Your earned career remains separate." : "CAREER PROFILE · Earn progress by completing events.";
                    ShowUnavailableCareerFormats(null, query.Result);
                    if(query.Result.Count==0) {hub.ShowMessage("No supported career events were found in these game files.");return;}
                    // RaceAndRide:Present: a career at 100 % that has not seen the completion yet shows the Career Completion page (PlayerEvent "CareerCompleted")
                    if(CareerCompletionDue()){CoreGarageProfile.SetEventStatus("CareerCompleted",1);ShowCareerCompletion(()=>ShowOwnedCareer(presentation));return;}
                    hub.Fill(query.Result,text);
                    var target=hub.DefaultFocus;if(target!=null)_nav.Focus(target);
                    if(focusSeries>=0)hub.FocusSeries(focusSeries);
                };
            }
            catch(Exception error){hub.ShowMessage(error.Message);}
        }
        // RaceAndRideEvents:OnAcceptSeries: a locked series shows the LockedPopup, a vehicle of another class the InvalidRaceRequirements popup, a Moto / Heat the
        // CareerPracticePopup, every other format the StartRacePopup (YES starts the event: the rules popup and the gate selection follow inside the event).
        bool _careerStarting;
        /// Tests: replaces the launch itself (the showcase must not leave the front end).
        public static Action<CareerEventChoice> CareerLaunchOverride;
        internal void ChooseCareerEvent(CareerEventChoice choice, IOwnCareerPresentationInstallation presentation, OcRaceAndRide hub)
        {
            if(_careerStarting)return;
            try {
                if(choice.Locked){OuSound.Play("Locked");ShowLockedPopup();return;}
                if(!choice.Eligible && choice.Supported && choice.UnownedRewardVehicles==0 && (presentation.Capabilities & choice.RequiredPresenters)==choice.RequiredPresenters){ShowInvalidVehiclePopup();return;}
                if(choice.Entry.IsMoto){ShowMotoPracticeChoice(choice,presentation);return;}
                if(choice.Entry.IsHeat){ShowHeatPracticeChoice(choice,presentation);return;}
                ShowStartRacePopup(choice,()=>
                {
                    _careerStarting=true;
                    try { if(CareerLaunchOverride!=null)CareerLaunchOverride(choice); else OwnCareerLaunch.Launch(choice,presentation); }
                    catch(Exception error){_careerStarting=false;hub.Note.text=error.Message;}
                });
            }
            catch(Exception error){hub.Note.text=error.Message;}
        }
        // The practice / qualifier pages: the career stage, one panel with the explanation and the choices, the plaque and the legend.
        VisualElement CareerChoicePage(string pageName,string title,string crumb,string message,out Label note)
        {
            Clear(Screen.Working,"career"); SetBackground(null); MusicTier(2); CareerChrome();
            OcUi.Stage(_layer,true);
            var mid=OuUi.Div("oc-mid",_layer);mid.style.justifyContent=Justify.Center;
            var panel=OcUi.Panel(mid,title,out var body,out _,"oc-choice");panel.name=pageName;panel.style.alignSelf=Align.Center;
            var tag=OuUi.Text(crumb,"oc-eyebrow",body);tag.enableRichText=false;tag.style.marginBottom=10;
            note=OuUi.Text(message,"oc-text",body);note.enableRichText=false;
            OcUi.Plaque(OuUi.Div("oc-bottom",_layer),OpenContent.Str("STR_CAREER","MOTOCAREER").ToUpperInvariant());
            return body;
        }
        // CareerPracticePopup.lua (RaceAndRideEvents:OnAcceptSeries, Moto and Heat alike): PRACTICE (ButtonOne, focused:
        // progress:SetSkipPractice( FALSE )) rides the practice qualifier first (MotoPractice / HeatPractice rules, three laps); its post
        // (MXSeriesEventMoto vf13 005af2c0 / MXSeriesEventHeat vf13 005adde0) writes each rider's best lap as the SortValue that orders the
        // gate picks (vf9 005a8a10 -> 005a9640). SKIP PRACTICE (ButtonTwo: SetSkipPractice( TRUE ); vf10 005aefc0 / 005ad960) resets the
        // local players' SortValue to FLT_MAX: the last pick. The core answers both (rx_session_career_moto_practice_choice).
        void ShowMotoPracticeChoice(CareerEventChoice choice, IOwnCareerPresentationInstallation presentation)=>
            ShowPracticeChoice(choice,presentation,"moto","BEFORE MOTO 1","Ride the practice qualifier to earn an earlier gate pick for Moto 1, or skip it and choose your gate from the ones left.");
        void ShowHeatPracticeChoice(CareerEventChoice choice, IOwnCareerPresentationInstallation presentation)=>
            ShowPracticeChoice(choice,presentation,"heat","BEFORE THE HEAT","Ride the practice qualifier to earn an earlier gate pick, or skip it and choose your gate for the heat.");
        void ShowPracticeChoice(CareerEventChoice choice, IOwnCareerPresentationInstallation presentation, string kind, string crumb, string message)
        {
            var page=CareerChoicePage("career-"+kind+"-practice-choice","PRACTICE",crumb,message,out var note);
            bool launching=false;
            VisualElement practice=null,skip=null;
            void Start(bool skipPractice)
            {
                if(launching)return;
                launching=true;practice.SetEnabled(false);skip.SetEnabled(false);
                try { OwnCareerLaunch.Launch(choice,presentation,motoSkipPractice:skipPractice); }
                catch(Exception error) { launching=false;practice.SetEnabled(true);skip.SetEnabled(true);note.text=error.Message;_nav.Focus(skipPractice?skip:practice); }
            }
            practice=OcUi.Button("PRACTICE","career-"+kind+"-practice",()=>Start(false),page);
            skip=OcUi.Button("SKIP PRACTICE AND CHOOSE GATE","career-"+kind+"-skip-practice",()=>Start(true),page);
            Action back=()=>{if(launching)return;ShowOwnedCareer(presentation);};
            var backButton=OcUi.Button("BACK TO CAREER","career-"+kind+"-practice-back",back,page,"oc-btn-quiet");
            var column=new List<VisualElement>{practice,skip,backButton};OuWidgets.LinkColumn(column);
            OcUi.Legend(_layer,(OuUi.KeySubmit,"SELECT",null),(OuUi.KeyCancel,"BACK",back));
            _nav.OnCancel=()=>{back();return true;};_nav.DefaultItem=()=>practice;_nav.Focus(practice);
        }
        // Series the core does not admit are listed with their rows (NOT AVAILABLE YET); the core's reason goes to the log.
        void ShowUnavailableCareerFormats(VisualElement parent, IReadOnlyList<CareerEventChoice> available)
        {
            foreach (var row in available)
                if (!row.Supported && !string.IsNullOrEmpty(row.Reason))
                    UnityEngine.Debug.Log("[OwnCareer] series " + row.Entry.SeriesId + " (" + row.Entry.Activity + " / " + row.Entry.Format + "): " + row.Reason);
        }
    }
}

using System;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.OriginalWorld
{
    public static class OwnCareerLaunch
    {
        sealed class PendingLaunch
        {
            public CareerEventEntry Entry;
            public CareerEventChoice Choice;
            public bool? MotoSkipPractice;
            public string EventKey;
            public IPlayerSessionPersistence Persistence;
            public IOwnCareerPresentationInstallation Presentation;
        }
        static PendingLaunch pending;
        internal static bool HasPending => pending != null;
        // Main-thread scene handoff only. Native Startup rechecks the current full profile's eligibility.
        public static void Launch(CareerEventChoice choice, IOwnCareerPresentationInstallation presentation, bool? motoSkipPractice = null)
        {
            if(choice==null || !choice.CanSelect || presentation==null) throw new InvalidOperationException("This career event is unavailable.");
            // CareerPracticePopup: a Moto or Heat event starts only with the player's own answer (PRACTICE or SKIP PRACTICE).
            if(choice.Entry.HasPracticeChoice && !motoSkipPractice.HasValue)throw new InvalidOperationException("Choose PRACTICE or SKIP PRACTICE before this "+choice.Entry.Format+" event.");
            if(!choice.Entry.HasPracticeChoice && motoSkipPractice.HasValue)throw new InvalidOperationException("A practice choice does not belong to this event.");
            if(pending!=null || UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>()!=null)
                throw new InvalidOperationException("Close the current ride before starting a career event.");
            var entry=CareerEventCatalog.Read().Single(x=>x.SeriesId==choice.Entry.SeriesId);
            if(entry.ProductId!=choice.Entry.ProductId || entry.TrackAsset!=choice.Entry.TrackAsset || entry.TrackId!=choice.Entry.TrackId
                || entry.Activity!=choice.Entry.Activity || entry.VehicleClass!=choice.Entry.VehicleClass || entry.Format!=choice.Entry.Format)
                throw new InvalidOperationException("The installed career event changed. Open career again.");
            var ev=ReflexEvents.All.SingleOrDefault(x=>x.location==entry.TrackAsset && x.spec.type==entry.Activity && x.spec.trackId==entry.TrackId);
            if(ev==null) throw new InvalidOperationException("The career location is unavailable.");
            // The presenter capabilities the core says this series' races need (its HUD route and the format's HUD elements).
            uint required=(uint)choice.RequiredPresenters;
            if(required==0 || ((uint)presentation.Capabilities&required)!=required)
                throw new InvalidOperationException("This career event's screens are unavailable.");
            // Capture the currently selected career/unlocked authority without changing the mode.
            pending=new PendingLaunch { Entry=entry,Choice=choice,EventKey=ev.key,Persistence=CoreGarageProfile.CapturePersistence(),Presentation=presentation,MotoSkipPractice=motoSkipPractice };
            SceneManager.sceneLoaded+=SceneLoaded;
            // the loading page under the career's pages until the world is drawn (never the race scene's world before its terrain is built)
            var loading=Reflex.Port.OpenFrontEnd.OpenLoading.ShowCareer(ev);
            try { ReflexEventSelect.Launch(ev.key,2,false,"Reflex",0,null); }
            catch { Clear(); if(loading) UnityEngine.Object.Destroy(loading.gameObject); throw; }
        }
        static void Clear() { pending=null; SceneManager.sceneLoaded-=SceneLoaded; }
        static void SceneLoaded(Scene scene,LoadSceneMode mode)
        {
            if(pending==null) return;
            // A missing scene hook is a failed handoff; never reuse the selection on a later scene.
            var runtime=UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if(runtime!=null) runtime.enabled=false;
            Clear(); Debug.LogError("[OwnCareer] The scene did not consume the selected career event.");
        }
        internal static bool Prepare(OriginalWorldRuntime runtime, ReflexEvents.Event ev)
        {
            var request=pending; if(request==null) return false;
            Clear();
            try
            {
                if(runtime==null || ev==null || ev.key!=request.EventKey || ev.location!=request.Entry.TrackAsset
                    || ev.spec.type!=request.Entry.Activity || ev.spec.trackId!=request.Entry.TrackId)
                    throw new InvalidOperationException("The loaded scene does not match the selected career event.");
                runtime.PrepareOwnedCareer(request.Choice,request.Persistence,request.Presentation,request.MotoSkipPractice);
                return true;
            }
            catch { if(runtime!=null) runtime.enabled=false; throw; }
        }
    }
}

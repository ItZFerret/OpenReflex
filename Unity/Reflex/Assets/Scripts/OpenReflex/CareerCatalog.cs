using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Reflex.Port.RxCore
{
    public sealed class CareerEventEntry
    {
        public readonly int SeriesId, ProductId, ProductLocalizationId, SortValue, LocationId, TrackId,
            TrackLocalizationId, VehicleClass, Laps, Riders, EventTypeId;
        public readonly string TrackAsset, Activity;
        // The authored SeriesFormat ("SingleEvent", "Moto", "Eliminator", ...) and SeriesAIFormat ("Matched", "Fixed", "Rival").
        public readonly string Format, AiFormat;
        public bool IsMoto => Format == "Moto";
        public bool IsHeat => Format == "Heat";
        /// The CareerPracticePopup before Start: Moto (skip only in the open product) and Heat (practice or skip).
        public bool HasPracticeChoice => IsMoto || IsHeat;
        public CareerEventEntry(int series, int product, int productName, int sort, int location, int track,
            int trackName, string asset, int vehicleClass, int laps, int riders, int eventType, string activity,
            string format = "SingleEvent", string aiFormat = "Matched")
        {
            SeriesId=series; ProductId=product; ProductLocalizationId=productName; SortValue=sort;
            LocationId=location; TrackId=track; TrackLocalizationId=trackName; TrackAsset=asset;
            VehicleClass=vehicleClass; Laps=laps; Riders=riders; EventTypeId=eventType; Activity=activity;
            Format=format; AiFormat=aiFormat;
        }
    }
    public sealed class CareerEventChoice
    {
        public readonly CareerEventEntry Entry;
        public readonly bool Authored, Supported, Locked, Eligible;
        public readonly int BlockingProductId;
        public readonly uint UnownedRewardVehicles;
        /// The player's best finish in the series (PlayerSeriesStatus.BestPlaceFinish): 0 gold, 1 silver, 2 bronze, -1 none (the career pages' trophies).
        public readonly int BestPlace;
        // From rx_session_career_series_presentation: the presenter capabilities the series' races need, the HUD route the original
        // requests for them, the activity mode of the format and, when the core does not admit the series, why.
        public readonly RxCareerPresenter RequiredPresenters;
        public readonly string HudRoute, Reason;
        public readonly uint ActivityMode;
        public bool CanSelect => Authored && Supported && !Locked && Eligible && UnownedRewardVehicles==0;
        internal CareerEventChoice(CareerEventEntry entry, RxCareerSeriesInfo value, RxCareerSeriesPresentation presentation, int bestPlace=-1)
        {
            Entry=entry; BestPlace=bestPlace>=0 && bestPlace<=2 ? bestPlace : -1; Authored=value.authored!=0; Supported=value.supported!=0; Locked=value.locked!=0;
            Eligible=value.eligible!=0; BlockingProductId=value.blocking_product_id;
            UnownedRewardVehicles=value.unowned_reward_vehicle_count;
            RequiredPresenters=(RxCareerPresenter)presentation.required_capabilities; HudRoute=presentation.HudRoute;
            Reason=presentation.Reason; ActivityMode=presentation.activity_mode;
        }
    }
    public sealed partial class OpenReflexSimulation
    {
        // The player's best finish per series, read from the profile the catalog was prepared with; a profile that cannot be read shows no trophies.
        static Dictionary<int,int> BestPlaces(byte[] profile)
        {
            var result=new Dictionary<int,int>();
            try
            {
                var table=PlayerSnapshotDocument.Decode(profile).Table("PlayerSeriesStatus");
                for(int row=0;row<table.RowCount;row++)
                {
                    var id=table.Get(row,"SeriesId");var place=table.Get(row,"BestPlaceFinish");
                    if(id.Type==PlayerSnapshotType.Int && place.Type==PlayerSnapshotType.Int) result[unchecked((int)id.Bits)]=unchecked((int)place.Bits);
                }
            }
            catch(Exception) { result.Clear(); }
            return result;
        }
        // Uses the same arena lease as a ride. Queries never overlap a live or closing session.
        // Prepare only copies/migrates in memory; no Imported/Checkpoint/Complete receipt is issued.
        public static Task<IReadOnlyList<CareerEventChoice>> ReadCareerCatalogAsync(string install,
            IEnumerable<CareerEventEntry> authored, IPlayerSessionPersistence profile,
            RxCareerPresenter availablePresenters, CancellationToken cancellation)
        {
            if (string.IsNullOrEmpty(install) || authored==null || profile==null) throw new ArgumentNullException();
            var rows=authored.ToArray();
            if (rows.Any(x=>x==null || x.SeriesId<0) || rows.Select(x=>x.SeriesId).Distinct().Count()!=rows.Length)
                throw new ArgumentException("Invalid career catalog identity.");
            return Task.Run<IReadOnlyList<CareerEventChoice>>(() =>
            {
                bool leased=false; IntPtr native=IntPtr.Zero;
                try
                {
                    cancellation.ThrowIfCancellationRequested();
                    var check=RxApiNative.Check();
                    if (!check.Compatible) throw new InvalidOperationException(check.Summary);
                    RxApiNative.RequirePlayerPersistence();
                    lock(sessionOwnership)
                    {
                        var wait=Stopwatch.StartNew();
                        while(activeSessions!=0)
                        {
                            cancellation.ThrowIfCancellationRequested();
                            if(arenaFailure!=null) throw new InvalidOperationException(arenaFailure);
                            if(wait.ElapsedMilliseconds>60000) throw new InvalidOperationException("The previous ride is still closing.");
                            Monitor.Wait(sessionOwnership,100);
                        }
                        cancellation.ThrowIfCancellationRequested();
                        if(arenaFailure!=null) throw new InvalidOperationException(arenaFailure);
                        activeSessions++; leased=true;
                    }
                    native=RxApiNative.rx_session_create(install);
                    if(native==IntPtr.Zero) throw new InvalidOperationException(RxApiNative.rx_session_last_error(native));
                    var selected=profile.Prepare(RxApiNative.ExportPlayer(native));
                    if(selected==null || RxApiNative.ImportPlayer(native,selected)!=0)
                        throw new InvalidOperationException("Career profile could not be read by the core.");
                    var medals=BestPlaces(selected);
                    var before=RxApiNative.ExportPlayer(native);
                    if(!selected.SequenceEqual(before)) throw new InvalidOperationException("Career profile readback differs.");
                    var config=RxCareerConfig.Create(availablePresenters);
                    if(RxCareerNative.Configure(native,ref config,out ulong generation)!=RxCareerResult.Ok)
                        throw new InvalidOperationException("Career catalog configuration failed.");
                    ulong frames=RxApiNative.rx_session_frames(native);
                    var result=new List<CareerEventChoice>();
                    foreach(var entry in rows)
                    {
                        cancellation.ThrowIfCancellationRequested();
                        var value=RxCareerSeriesInfo.Create();
                        var rc=RxCareerNative.Series(native,generation,(uint)entry.SeriesId,ref value);
                        if(rc!=RxCareerResult.Ok || value.series_id!=(uint)entry.SeriesId)
                            throw new InvalidOperationException("Career eligibility query failed: "+rc);
                        if(value.authored!=0 && (value.product_id!=entry.ProductId || value.vehicle_class!=entry.VehicleClass
                            || value.event_type_id!=entry.EventTypeId || value.Owner!="Tour" || value.Format!=entry.Format || value.Activity!=entry.Activity))
                            throw new InvalidOperationException("The career catalog differs from the loaded core data.");
                        var presentation=RxCareerSeriesPresentation.Create();
                        rc=RxCareerNative.SeriesPresentation(native,generation,(uint)entry.SeriesId,ref presentation);
                        if(rc!=RxCareerResult.Ok || presentation.series_id!=(uint)entry.SeriesId || (presentation.admitted!=0)!=(value.supported!=0))
                            throw new InvalidOperationException("Career presentation query failed: "+rc);
                        result.Add(new CareerEventChoice(entry,value,presentation,medals.TryGetValue(entry.SeriesId,out var place) ? place : -1));
                    }
                    var status=RxCareerStatus.Create();
                    if(RxCareerNative.Status(native,generation,ref status)!=RxCareerResult.Ok
                        || status.phase!=(uint)RxCareerPhase.Configured || status.event_present!=0 || status.activity_present!=0
                        || status.pending_requests!=0 || status.records_finalized!=0 || status.checkpoint_error!=0
                        || frames!=RxApiNative.rx_session_frames(native) || !before.SequenceEqual(RxApiNative.ExportPlayer(native)))
                        throw new InvalidOperationException("Career catalog query changed session state.");
                    return new ReadOnlyCollection<CareerEventChoice>(result);
                }
                finally
                {
                    try { if(native!=IntPtr.Zero) RxApiNative.rx_session_destroy(native); }
                    catch(Exception e)
                    {
                        lock(sessionOwnership) arenaFailure="The career catalog session could not close safely. Please restart OpenReflex. "+e.Message;
                        throw;
                    }
                    finally { if(leased) lock(sessionOwnership) { activeSessions--; Monitor.PulseAll(sessionOwnership); } }
                }
            },cancellation);
        }
    }
}

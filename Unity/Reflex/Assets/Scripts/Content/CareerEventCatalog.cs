using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    public static class CareerEventCatalog
    {
        // Metadata only. Actual profile eligibility is always queried from the core.
        public static IReadOnlyList<CareerEventEntry> Read()
        {
            var tables=ReflexEvents.Tables;
            List<Dictionary<string,object>> Rows(string name) => tables.TryGetValue(name,out var rows)
                ? rows : throw new InvalidDataException("Career table is missing: "+name);
            Dictionary<string,object> One(string table,string key,int id)
            {
                var rows=Rows(table).Where(x=>Rfx.Int(x,key)==id).ToArray();
                if(rows.Length!=1) throw new InvalidDataException("Career table identity is missing or duplicated: "+table);
                return rows[0];
            }
            var result=new List<CareerEventEntry>();
            foreach(var series in Rows("Series"))
            {
                // The Race & Ride events page lists the Tour series of its tier product (RaceAndRideEvents.lua). The tier product
                // 2904 opens the challenges page instead (RaceAndRide.lua), whose series start a plain Free Ride: not career events.
                // Which of these the core runs is the core's answer (rx_session_career_series / _presentation), never a list here.
                if(Rfx.Str(series,"SeriesOwner")!="Tour") continue;
                int id=Rfx.Int(series,"SeriesId"),eventType=Rfx.Int(series,"EventTypeId");
                int productId=Rfx.Int(series,"StoreProductId"); var product=One("StoreProducts","StoreProductId",productId);
                if(Rfx.Int(product,"ParentStoreProductId")==2904) continue;
                var type=One("EventType","EventTypeId",eventType); string activity=Rfx.Str(type,"ActivityName");
                var locations=Rows("SeriesEventLocations").Where(x=>Rfx.Int(x,"SeriesId")==id).OrderBy(x=>Rfx.Int(x,"EventNumber")).ToArray();
                var rules=Rows("SeriesEventRules").Where(x=>Rfx.Int(x,"SeriesId")==id).ToArray();
                if(locations.Length<1 || rules.Length!=1) continue;
                var rule=rules[0];
                int locationId=Rfx.Int(locations[0],"EventLocationId"),track=Rfx.Int(locations[0],"TrackId");
                var location=One("EventLocation","EventLocationId",locationId);
                int laps=Rfx.Int(rule,"Duration"),riders=Rfx.Int(rule,"NumRiders");
                string asset=Rfx.Str(location,"Filename");
                if(id<0 || productId<0 || track<0 || laps<0 || riders<1 || riders>24 || string.IsNullOrEmpty(asset))
                    throw new InvalidDataException("Career event metadata is invalid: series "+id+".");
                result.Add(new CareerEventEntry(id,productId,Rfx.Int(product,"LocalizationId"),Rfx.Int(series,"SortValue"),
                    locationId,track,Rfx.Int(location,"LocalizationId"),asset,Rfx.Int(series,"VehicleClassId"),laps,riders,eventType,activity,
                    Rfx.Str(series,"SeriesFormat"),Rfx.Str(series,"SeriesAIFormat")));
            }
            if(result.Select(x=>x.SeriesId).Distinct().Count()!=result.Count) throw new InvalidDataException("Duplicate career event.");
            return new ReadOnlyCollection<CareerEventEntry>(result.OrderBy(x=>x.SortValue).ThenBy(x=>x.SeriesId).ToList());
        }
    }
}

using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    // The game modes' front-end pages (Content/ReflexModes.cs, _re/notes/modes.md).
    public static class FeModeScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "RaceAndRideChallenges": return new RaceAndRideChallengesScript();
                default: return null;
            }
        }
    }

    // RaceAndRideChallenges.lua (Race and Ride Challenges Page, from Race and Ride's Free Ride group 2904): the group's Tour series
    // (one per Free Ride level: FR_Alpine, FR_Gibson, FR_Cheyenne) as posters (the series' store product IconId, lock / new marks,
    // the trophy of PlayerSeriesStatus.BestPlaceFinish); the info: LocationNameText = the series' title, LocationImageComponent =
    // the level's Free Ride preview (EventTypeLocations type 9 PreviewIconId), and "Challenge 1".."Challenge 4" = the level's
    // challenge rows (EventTypeLocations whose type IsChallenge, Free Ride excluded) with the player's medal (PlayerChallenges by
    // EventTypeLocationId: Gold / Silver / Bronze). Focus on a new series: SetStoreProductNotNew. Accept: locked -> the locked popup;
    // else EventType FreeRide at the series' location with SeriesId (SetupEvent / StartEvent: ReflexModes' Free Ride of the series, where
    // the activation areas start the challenges). Back -> CareerModeMenuPage; QuickView (Toggle: VehiclesQuickViewPopup) not ported.
    public sealed class RaceAndRideChallengesScript : FeScript
    {
        sealed class Item { public int seriesId, product, placing, location; public bool locked, isNew; }
        readonly List<Item> data = new List<Item>(); FePosterList posters; FeNode focusedItem, locationName, locationImage, challengeContainer, leftArrow, rightArrow;
        int version = -1, parent = -1;
        public override void Initialize()
        {
            posters = new FePosterList(C("Event Select List"));
            focusedItem = C("FocusedItem"); locationName = C("LocationNameText"); locationImage = C("LocationImageComponent");
            challengeContainer = C("Challenge Container"); leftArrow = C("Left Arrow"); rightArrow = C("Right Arrow");
            functions["OnAcceptChallenge"] = OnAcceptChallenge;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("CareerModeMenuPage"); };
            functions["OnQuickViewKeyPress"] = () => { };
            posters.list.onListChanged = (l, old) => { posters.Scrolled(l.absIndex - old); OnEventListFocusedChanged(); };
        }
        public override void Start()
        {
            fe.SetButtonLegend("accept", true, "STR_SELECT"); fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("toggle", false, "STR_QUICKVIEW"); fe.SetButtonLegend("misc", false, "STR_OPTIONS");
            parent = fe.Var("SeriesStoreProductId", ReflexCareer.SeriesStoreProductId);
            fe.SetHeader(FeTables.Shared.TitleKey(parent) ?? "STR_RACEANDRIDE");   // DYN_HEADER = the group's title
            Build(true);
        }
        public override void Update(float dt)
        {
            posters.Update(dt);
            if (ReflexProfile.Version != version && !ReflexProfile.Busy) Build(false);
        }
        public int Count => data.Count;

        void Build(bool reset)
        {
            version = ReflexProfile.Version;
            var p = FeCareerScripts.Profile; data.Clear();
            var t = FeTables.Shared;
            foreach (var r in t.Rows("Series"))   // Series of SeriesOwner "Tour" under the group's store product
            {
                int sp = FeTables.I(r, "StoreProductId");
                if (FeTables.S(r, "SeriesOwner") != "Tour" || FeTables.I(t.Rows("StoreProducts").FirstOrDefault(s => FeTables.I(s, "StoreProductId") == sp), "ParentStoreProductId") != parent) continue;
                int id = FeTables.I(r, "SeriesId");
                var status = p?.T("PlayerSeriesStatus"); int row = status != null ? status.Find("SeriesId", id) : -1;
                data.Add(new Item
                {
                    seriesId = id, product = sp, locked = p != null && p.IsLocked(sp), isNew = p != null && p.IsNew(sp),
                    placing = row < 0 ? 1000 : status.Int("BestPlaceFinish", row), location = FeCareerScripts.Lookup("SeriesEventLocations", id, "EventLocationId"),
                });
            }
            posters.list.numItems = data.Count;
            if (reset) posters.list.absIndex = 0;
            posters.list.absIndex = Mathf.Clamp(posters.list.absIndex, 0, Math.Max(0, data.Count - 1));
            OnEventListFocusedChanged();
        }

        void Format(FeNode c, int d)
        {
            var it = data[d];
            FeCareerScripts.SetIcon(fe, c.Find("EventImage"), FeTables.I(FeTables.Shared.Rows("StoreProducts").FirstOrDefault(r => FeTables.I(r, "StoreProductId") == it.product), "IconId"));
            if (c.Find("LockImage") is FeNode l) l.Visible = it.locked;
            if (c.Find("NewImage") is FeNode n) n.Visible = it.isNew;
            if (c.Find("Gold_ImageComponent") is FeNode g) g.Visible = it.placing == 0;
            if (c.Find("Silver_ImageComponent") is FeNode s) s.Visible = it.placing == 1;
            if (c.Find("Bronze_ImageComponent") is FeNode b) b.Visible = it.placing == 2;
        }

        void OnEventListFocusedChanged()
        {
            posters.Format(Format);
            if (data.Count == 0) { focusedItem.Visible = false; return; }
            var it = data[posters.list.absIndex];
            layer.Stop("FocusedItem_Changed"); layer.Play("FocusedItem_Changed");
            focusedItem.Visible = true; Format(focusedItem, posters.list.absIndex);
            int i = posters.list.absIndex, n = posters.list.numItems; leftArrow.Visible = i > 0; rightArrow.Visible = i < n - 1;   // UpdateArrows
            if (it.isNew) ReflexProfile.Run("store_not_new " + it.product);
            UpdateChallengeDisplay(it);
        }

        void UpdateChallengeDisplay(Item it)
        {
            var t = FeTables.Shared;
            locationName?.SetText(Str(t.TitleKey(it.product) ?? ""));
            var free = t.Rows("EventTypeLocations").FirstOrDefault(r => FeTables.I(r, "EventLocationId") == it.location && FeTables.I(r, "EventTypeId") == 9);
            FeCareerScripts.SetIcon(fe, locationImage, FeTables.I(free, "PreviewIconId"));
            var challengeTypes = new HashSet<int>(t.Rows("EventType").Where(r => FeTables.I(r, "IsChallenge", 0) == 1).Select(r => FeTables.I(r, "EventTypeId")));
            var rows = t.Rows("EventTypeLocations").Where(r => FeTables.I(r, "EventLocationId") == it.location && challengeTypes.Contains(FeTables.I(r, "EventTypeId")) && FeTables.I(r, "EventTypeId") != 9).ToList();
            var player = FeCareerScripts.T("PlayerChallenges");
            for (int k = 1; k <= 4; k++)
            {
                var c = challengeContainer?.Find("Challenge " + k); if (c == null) continue;
                c.Visible = k <= rows.Count;
                if (k > rows.Count) continue;
                int etl = FeTables.I(rows[k - 1], "EventTypeLocationId"), pr = player.Find("EventTypeLocationId", etl);
                bool gold = pr >= 0 && player.Int("Gold", pr) >= 1, silver = !gold && pr >= 0 && player.Int("Silver", pr) >= 1, bronze = !gold && !silver && pr >= 0 && player.Int("Bronze", pr) >= 1;
                if (c.Find("Gold") is FeNode g) g.Visible = gold;
                if (c.Find("Silver") is FeNode s) s.Visible = silver;
                if (c.Find("Bronze") is FeNode b) b.Visible = bronze;
            }
        }

        void OnAcceptChallenge()
        {
            if (data.Count == 0) return;
            var it = data[posters.list.absIndex];
            if (it.locked) { fe.PlaySound("Locked"); fe.SendUIEvent("ShowLockedPopup"); return; }
            var ev = ReflexModes.SeriesFreeRide(it.location);
            if (ev == null) { Debug.Log("[Challenges] no Free Ride for location " + it.location); fe.SendUIEvent("ShowLockedPopup"); return; }
            fe.PlaySound("Accept");
            // SetupEvent / StartEvent / GotoLoadingPage; seriesManager:DisableSeriesMode (no series event runs). As a career page: the
            // arcade's remembered event is left alone, and the port comes back to this page
            fe.LaunchCareerEvent(ev); FrontEndManager.ReturnPage = fe.CurrentPage;
        }
    }
}

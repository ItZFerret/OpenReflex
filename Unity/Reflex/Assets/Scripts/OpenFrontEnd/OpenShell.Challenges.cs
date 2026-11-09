// The Race & Ride CHALLENGES page of the open shell: RaceAndRideChallenges.lua ("Race and Ride Challenges Page", the Free Ride group, store product 2904),
// in the career menu's look (OuCareerKit, OuP2Front.uss) and fully on the pad. The original shows the group's three Free Ride series (FR_Alpine, FR_Gibson,
// FR_Cheyenne) as posters; the focused one shows its Free Ride photo, its name and a medal for each of the level's challenges (Hill Climb, Hit Discovery,
// Target Practice, Trailblazer); the accept key starts the series' Free Ride (SeriesId set: the activation areas start the challenges there) and a locked
// series answers with the LockedPopup. Here each challenge is also a row of its own with the medal thresholds PreChallengePopup lists, the player's best
// value and a START that rides that challenge at once (the world the activation area would start); the Hit Discovery row and the RIDE button start the
// Free Ride itself. LB / RB (Q / E) change the location; Back goes to the Free Ride location browser it was opened from.
//   reached  FREE RIDE (hub) > CHALLENGES (the browser's chip, ou-challenges)
//   names    challenge-location-<SeriesId> (the location cards), challenge-<Activity>-<SeriesId> (the rows), challenge-ride-<SeriesId> (the Free Ride)
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        // where the race scene returns to: the page again, on the same location and row
        static bool s_returnChallenges;
        static int s_returnChallengeSeries = -1; static string s_returnChallengeRow;

        sealed class ChallengePage
        {
            public List<OpenChallengeLocation> locations;
            public readonly List<VisualElement> cards = new List<VisualElement>(), details = new List<VisualElement>();
            public readonly List<List<VisualElement>> rows = new List<List<VisualElement>>();
            public int current = -1;
            public Label description;
        }
        ChallengePage _challenges;

        /// The page is up (tests).
        public bool ChallengesOpen => _challenges != null && Current == Screen.Working && _layer.Q("challenges-page") != null;

        /// The browser's CHALLENGES chip (FREE RIDE only): the original reaches the page from Race & Ride's Free Ride poster.
        void BrowserChallengesChip(VisualElement tools)
        {
            var chip = OuUi.Button("CHALLENGES", "ou-chip", () => ShowChallenges(), tools);
            chip.name = "ou-challenges";
        }

        /// Called from Start(): back from a race the page started.
        bool ChallengesReturn()
        {
            if (!s_returnChallenges) return false;
            s_returnChallenges = false;
            _heroWalked = true;
            ShowChallenges(s_returnChallengeSeries, s_returnChallengeRow);
            return true;
        }

        public void ShowChallenges(int focusSeries = -1, string focusRow = null, ProfileData profile = null)
        {
            Clear(Screen.Working, "career"); SetBackground(null); MusicTier(5); CareerChrome();
            var p = new ChallengePage(); _challenges = p;
            if (profile == null) ReflexProfile.Open();   // (the profile again: the medals of the race just ridden)
            var data = profile ?? ReflexProfile.Current;
            try { p.locations = OpenChallengeData.Read(data); }
            catch (Exception e) { Debug.LogWarning("[Challenges] " + e); p.locations = new List<OpenChallengeLocation>(); }
            OcUi.Stage(_layer, true);
            var page = OuUi.Div("p2-ch", _layer); page.name = "challenges-page";
            OcUi.Header(page, OpenChallengeData.GroupTitle(), OpenContent.Str("STR_RACEANDRIDE", "RACE & RIDE").ToUpperInvariant());
            var mid = OuUi.Div("p2-ch-mid", page);
            var column = OuUi.Div("p2-ch-locs", mid);
            var side = OuUi.Div("p2-ch-side", mid);
            Action back = () => ShowBrowser(true);
            if (p.locations.Count == 0)
            {
                OuUi.Text("No Free Ride challenges were found in these game files.", "p2-ch-empty", mid).enableRichText = false;
                OcUi.Legend(_layer, (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant(), back));
                _nav.OnCancel = () => { back(); return true; };
                return;
            }
            for (int i = 0; i < p.locations.Count; i++) BuildChallengeLocation(p, i, column, side);
            // the focused challenge's STR_<ACTIVITY>_DESCRIPTION under the cards
            p.description = OuUi.Text("", "p2-ch-desc", column); p.description.enableRichText = false; p.description.name = "challenge-description";
            // up / down between the cards, right into the rows, left back to the card
            for (int i = 0; i < p.cards.Count; i++)
            {
                var it = (OuNavItem)p.cards[i].userData;
                it.up = p.cards[Math.Max(0, i - 1)]; it.down = p.cards[Math.Min(p.cards.Count - 1, i + 1)];
                it.right = p.rows[i].Count > 0 ? p.rows[i][0] : null;
                foreach (var row in p.rows[i]) ((OuNavItem)row.userData).left = p.cards[i];
            }
            OcUi.Plaque(OuUi.Div("oc-bottom", _layer), OpenContent.Str("STR_RACEANDRIDE", "RACE & RIDE").ToUpperInvariant());
            OcUi.Legend(_layer, (OuUi.KeySubmit, "START", null), (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant(), back), (OuUi.KeyTabs, "LOCATION", null));
            _nav.OnCancel = () =>
            {
                var f = _nav.Focused;
                if (f != null && p.current >= 0 && !p.cards.Contains(f)) { _nav.Focus(p.cards[p.current]); return true; }   // from a row: the location again
                back(); return true;
            };
            _nav.OnTab = d => StepChallengeLocation(p, d);
            int start = Math.Max(0, p.locations.FindIndex(l => l.seriesId == focusSeries));
            _nav.DefaultItem = () => p.cards[Math.Max(0, p.current)];
            SelectChallengeLocation(p, start);
            VisualElement target = p.cards[start];
            if (focusRow != null) target = p.rows[start].FirstOrDefault(r => r.name == focusRow) ?? target;
            _nav.Focus(target);
            foreach (var l in p.locations.Where(l => l.isNew)) CoreGarageProfile.Command("store_not_new " + l.product);   // OnEventListFocusedChanged: SetStoreProductNotNew
        }

        void BuildChallengeLocation(ChallengePage p, int index, VisualElement column, VisualElement side)
        {
            var loc = p.locations[index];
            // the card (the poster's place in the original's list)
            var card = OuUi.Item(OuUi.Div("p2-ch-card", column), () => { OuSound.Play("Accept"); StartChallengeFreeRide(loc, null); });
            card.name = "challenge-location-" + loc.seriesId;
            var thumb = OuUi.Div("p2-ch-thumb", card); thumb.pickingMode = PickingMode.Ignore;
            if (loc.poster) thumb.style.backgroundImage = loc.poster; else if (loc.photo) thumb.style.backgroundImage = loc.photo;
            var copy = OuUi.Div("p2-ch-card-copy", card); copy.pickingMode = PickingMode.Ignore;
            OuUi.Text(loc.title, "p2-ch-card-name", copy).enableRichText = false;
            var slots = OuUi.Div("p2-ch-card-slots", copy); slots.pickingMode = PickingMode.Ignore;
            for (int k = 0; k < Math.Max(4, loc.challenges.Count); k++)
            {
                if (k >= loc.challenges.Count) { OcUi.Slot(slots, 0, -1, false, true); continue; }
                var c = loc.challenges[k];
                OcUi.Slot(slots, k + 1, c.medal == 3 ? 0 : c.medal == 2 ? 1 : c.medal == 1 ? 2 : -1, loc.locked);
            }
            var tags = OuUi.Div("p2-ch-card-tags", card); tags.pickingMode = PickingMode.Ignore;
            if (loc.locked) OcUi.Tag(tags, OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED").ToUpperInvariant(), "warn");
            else if (loc.isNew) OcUi.Tag(tags, "NEW", "orange");
            if (loc.challenges.Count > 0 && loc.Medals == loc.challenges.Count && loc.challenges.All(c => c.medal == 3)) { var cup = OuUi.Div("p2-ch-cup", tags); cup.style.backgroundImage = OcUi.Trophy(0); cup.pickingMode = PickingMode.Ignore; }
            int i = index;
            card.RegisterCallback<FocusInEvent>(_ => SelectChallengeLocation(p, i));
            p.cards.Add(card);

            // the detail panel of the location
            var panel = OcUi.Panel(side, loc.title, out var body, out _, "p2-ch-panel p2-ch-off");
            panel.name = "challenge-detail-" + loc.seriesId;
            var photo = OuUi.Div("p2-ch-photo", body); photo.pickingMode = PickingMode.Ignore;
            if (loc.photo) photo.style.backgroundImage = loc.photo;
            var head = OuUi.Div("p2-ch-photo-cap", photo); head.pickingMode = PickingMode.Ignore;
            OuUi.Text(loc.level == null ? "" : OpenContent.Str("STR_MXACTIVITYTYPE_FREERIDE", "FREE RIDE").ToUpperInvariant(), "p2-ch-eyebrow", head).enableRichText = false;
            var list = OuUi.Div("p2-ch-list", body);
            var rows = new List<VisualElement>();
            foreach (var ch in loc.challenges)
            {
                var c = ch;
                var row = OuUi.Item(OuUi.Div("p2-ch-row", list), () => StartChallenge(loc, c));
                row.name = "challenge-" + c.activity + "-" + loc.seriesId;
                OcUi.Slot(row, rows.Count + 1, c.medal == 3 ? 0 : c.medal == 2 ? 1 : c.medal == 1 ? 2 : -1, loc.locked, false, "p2-ch-slot");
                var text = OuUi.Div("p2-ch-row-copy", row); text.pickingMode = PickingMode.Ignore;
                OuUi.Text(c.title, "p2-ch-row-title", text).enableRichText = false;
                OuUi.Text(c.hasRecord ? "BEST  " + c.Format(c.record) : "NO RECORD YET", "p2-ch-row-best", text).enableRichText = false;
                var tiers = OuUi.Div("p2-ch-tiers", row); tiers.pickingMode = PickingMode.Ignore;
                string[] names = { "gold", "silver", "bronze" };
                for (int m = 0; m < 3; m++)
                {
                    int crit = 2 - m;   // criteria are bronze, silver, gold
                    var tile = OuUi.Div("p2-ch-tier p2-ch-tier-" + names[m] + (c.medal >= 3 - m ? " p2-ch-tier-got" : ""), tiers); tile.pickingMode = PickingMode.Ignore;
                    OuUi.Text(OwnPostChallenge.Str("STR_" + names[m].ToUpperInvariant(), names[m]).ToUpperInvariant(), "p2-ch-tier-name", tile).enableRichText = false;
                    OuUi.Text(c.criteria != null ? c.Format(c.criteria[crit]) : "-", "p2-ch-tier-value", tile).enableRichText = false;
                }
                var go = OuUi.Text(c.ev == null ? "-" : loc.locked ? OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED").ToUpperInvariant() : "START", "p2-ch-go", row); go.pickingMode = PickingMode.Ignore;
                if (c.ev == null) row.SetEnabled(false);
                var desc = c.description;
                row.RegisterCallback<FocusInEvent>(_ => { p.description.text = desc; });
                rows.Add(row);
            }
            // the series' Free Ride (RaceAndRideChallenges:OnAcceptChallenge)
            var ride = OcUi.Button(OpenContent.Str("STR_MXACTIVITYTYPE_FREERIDE", "FREE RIDE").ToUpperInvariant() + "  ·  " + loc.title, "challenge-ride-" + loc.seriesId, () => StartChallengeFreeRide(loc, null), body, "p2-ch-ride");
            ride.RegisterCallback<FocusInEvent>(_ => { p.description.text = OwnPostChallenge.Str("STR_RULE_DESCRIPTION_FREERIDE", ""); });
            rows.Add(ride);
            OuWidgets.LinkColumn(rows);
            foreach (var r in rows) ((OuNavItem)r.userData).right = null;
            p.rows.Add(rows);
            p.details.Add(panel);
        }

        void SelectChallengeLocation(ChallengePage p, int index)
        {
            index = Mathf.Clamp(index, 0, p.details.Count - 1);
            if (p.current == index) return;
            p.current = index;
            for (int i = 0; i < p.details.Count; i++)
            {
                p.details[i].EnableInClassList("p2-ch-off", i != index);
                p.cards[i].EnableInClassList("p2-ch-card-on", i == index);
            }
            p.description.text = p.locations[index].challenges.FirstOrDefault()?.description ?? "";
        }

        void StepChallengeLocation(ChallengePage p, int d)
        {
            int i = Mathf.Clamp(p.current + d, 0, p.cards.Count - 1);
            if (i == p.current) return;
            _nav.Focus(p.cards[i]);
        }

        // ------------------------------------------------------------------------------------------------ starting

        /// RaceAndRideChallenges:OnAcceptChallenge for the series: a locked one answers with the LockedPopup, else the series' Free Ride starts.
        void StartChallengeFreeRide(OpenChallengeLocation loc, OpenChallenge from)
        {
            if (loc.locked) { OuSound.Play("Locked"); ShowLockedPopup(); return; }
            var ev = loc.freeRide;
            if (ev == null) { Debug.Log("[Challenges] no Free Ride for location " + loc.locationId); ShowLockedPopup(); return; }
            LaunchChallengeWorld(loc, ev, from != null ? "challenge-" + from.activity + "-" + loc.seriesId : "challenge-ride-" + loc.seriesId);
        }

        /// A challenge row: the challenge's own world (Hit Discovery: the Free Ride).
        void StartChallenge(OpenChallengeLocation loc, OpenChallenge c)
        {
            if (loc.locked) { OuSound.Play("Locked"); ShowLockedPopup(); return; }
            if (c.ev == null) { OuSound.Play("Locked"); return; }
            // the Free Ride of the challenges shows the Freeride rules first (RulesPopup: InCareerMode), once
            if (c.IsFreeRide && RulesDue("Freeride")) { ShowRulesPopup("Freeride", () => LaunchChallengeWorld(loc, c.ev, "challenge-" + c.activity + "-" + loc.seriesId)); return; }
            LaunchChallengeWorld(loc, c.ev, "challenge-" + c.activity + "-" + loc.seriesId);
        }

        void LaunchChallengeWorld(OpenChallengeLocation loc, ReflexEvents.Event ev, string row)
        {
            Debug.Log("[Challenges] starting " + ev.key + " (series " + loc.seriesId + ")");
            s_returnChallenges = true; s_returnChallengeSeries = loc.seriesId; s_returnChallengeRow = row;
            s_returnEvent = null; s_returnFreeRide = false;
            Clear(Screen.Loading);
            _hints.Clear();
            _sounds?.music?.SetEnable(false);   // InGameLoadingPage:Start: App.SetEnableUIMusic( FALSE )
            if (ChallengeLaunchOverride != null) { ChallengeLaunchOverride(ev); return; }
            OwnAaaLoading.Show();
            // the Race & Ride challenges Free Ride runs with the career flag; the world is booted with the series' UI variables (ReflexModes)
            ReflexEventSelect.Launch(ev.key, 0, false, "Reflex", 0, null);
        }

        /// Tests: replaces the hand-over to the race scene.
        public static Action<ReflexEvents.Event> ChallengeLaunchOverride;
    }
}

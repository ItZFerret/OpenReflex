// C# ports of the front end's Lua page scripts (MXUI "script" assets; reference exports in _re/formats/out/ui/*.lua). Function
// names follow the Lua so the layers' own triggers (UIKeypressTrigger ScriptFunction=...) call them. The profile is the game's
// own (ReflexProfile, FrontEndCareer.cs): the title's OnStartPressed takes the PC path of a signed-in profile (Load Game), and
// MOTOCAREER / MOTOCARD follow MXProfile:IsSignedIn; without a profile session they fall back to the original's "no profile signed
// in" values (the profile reminder, the Motocard Required popup, MOTOCARD not focusable). MULTIPLAYER shows the network notification.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FeScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "Background": return new BackgroundScript();
                case "Title": return new TitleScript();
                case "MainMenu": return new MainMenuScript();
                case "DemoMainMenu": return new DemoMainMenuScript();
                case "HeaderLayer": return new HeaderLayerScript();
                case "EventSelect": return new EventSelectScript();
                case "LocationSelect": return new LocationSelectScript();
                case "VehicleSelect": return new VehicleSelectScript();   // FrontEndVehicleSelect.cs
                case "QuitConfirmationPopup": return new QuitConfirmationPopupScript();
                case "OptionsFrontEnd": return new OptionsFrontEndScript();
                case "ControllerRemapping": return new ControllerRemappingScript();   // FrontEndRemap.cs (WP-C)
                case "ProfileReminderPopup": return new OneButtonPopupScript("STR_PROFILEREMINDER_TITLE", "STR_PROFILEREMINDER_MESSAGE", "STR_CONTINUE", fe => fe.SendUIEvent("Main Menu Page"));
                case "ProfileRequiredPopup": return new OneButtonPopupScript("STR_PROFILEREQUIRED_TITLE", "STR_PROFILEREQUIRED_MESSAGE", "STR_CONTINUE", null);
                case "NetworkNotificationPopup": return new OneButtonPopupScript("STR_NETWORK_NOTIFICATION_TITLE", "STR_NETWORK_DSC_ONL_FEA_MESSAGE", "STR_CONTINUE", null);   // NetworkNotification = "DisconnectedOnlineFeature"
                default: return FeVideoScripts.Create(script) ?? FePopupScripts.Create(script) ?? FeCareerScripts.Create(script) ?? FeModeScripts.Create(script) ?? FeTutorialScripts.Create(script);   // (WP-T: TutorialFirstTimeEntryPopup, FrontEndTutorials.cs) the video layers (FrontEndVideo.cs), popups / overlay (FrontEndPopups.cs), MOTOCAREER / motocard pages (FrontEndCareer.cs, FrontEndMotocard.cs), the modes' pages (FrontEndChallenges.cs)
            }
        }
    }

    // Background.lua: Start plays "Loop1"; its UIAnimationTrigger calls OnAnimationLoop, which plays it again.
    public sealed class BackgroundScript : FeScript
    {
        public override void Initialize() { functions["OnAnimationLoop"] = () => layer.Play("Loop1"); }
        public override void Start() => layer.Play("Loop1");
    }

    // HeaderLayer.lua: SetHeader(key) sets "Header Text" (the shadow and the orange copy) by key.
    public sealed class HeaderLayerScript : FeScript
    {
        public void SetHeader(string key) { Set(layer.root, Str(key)); }
        static void Set(FeNode n, string s) { foreach (var k in n.kids) { if (k.name == "Header Text") k.SetText(s); Set(k, s); } }
    }

    // DemoMainMenu.lua: not the demo build -> the layer is hidden for good.
    public sealed class DemoMainMenuScript : FeScript
    {
        public override void Initialize() { layer.root.Visible = false; }
    }

    // Title.lua. Start: the Title Image hidden, the Title Video (Videos\Title.bik with its alpha, FrontEndVideo.cs) shown and played;
    // its DoneEvent TitleVideoDone (the layer's UIEventTrigger) -> OnTitleVideoDone: the Title Image (RavenLogo) instead. OnStartPressed
    // (Start / Accept, PC path): no profile is signed in -> ShowProfileReminderPopup. Back -> quit popup. The layer's 30 s
    // UIElapsedTimeTrigger -> OnAttractModeTrigger: no popup -> "Attract Page" (Attract.bik).
    public sealed class TitleScript : FeScript
    {
        public override void Initialize()
        {
            functions["OnStartPressed"] = OnStartPressed;
            functions["OnBackPressed"] = () => fe.SendUIEvent("ShowQuitConfirmationPopup");
            functions["OnTitleVideoDone"] = OnTitleVideoDone;
            functions["OnAttractModeTrigger"] = () => { if (fe.TopPopup == null && !fe.Busy) fe.SendUIEvent("Attract Page"); };
        }
        public override void Start()
        {
            fe.SetUiMusicTier(2); fe.EnableUiMusic(true);      // App.SetUIMusicTier( 2 ); App.SetEnableUIMusic( TRUE )
            fe.vars["PlayerMode"] = "None";
            if (C("Title Image") is FeNode i) i.Visible = false;
            var video = FeVideo.Of(C("Title Video"));
            if (video != null) { video.node.Visible = true; video.Play(fe); } else OnTitleVideoDone();
            KeyGlyph.Refresh(fe, layer.root);
        }
        void OnTitleVideoDone() { if (C("Title Image") is FeNode i) i.Visible = true; if (C("Title Video") is FeNode v) v.Visible = false; }
        void OnStartPressed()
        {
            // PC: the profile is signed in (the offline Steam user, ReflexProfile) -> RequestNewStorageDevice -> "Load Game"
            // (TitleLoadGame: the game loads the save; without a profile session the guest path, the profile reminder).
            fe.PlaySound("Accept");
            fe.SendUIEvent("Load Game");
        }
    }

    // MainMenu.lua: the four-item LeftRight list ARCADE / MOTOCAREER / MULTIPLAYER / MOTOCARD with the <Item>_Focused / _Unfocused
    // animations on focus changes; legend BACK / OPTIONS / MOTOSKILLS / QUIT (dev hidden in RELEASE); header MAIN MENU.
    public sealed class MainMenuScript : FeScript
    {
        FeNode list;
        static readonly string[][] Data = { new[] { "Arcade_Focused", "Arcade_Unfocused" }, new[] { "Career_Focused", "Career_Unfocused" }, new[] { "Multiplayer_Focused", "Multiplayer_Unfocused" }, new[] { "Motocard_Focused", "Motocard_Unfocused" } };
        public override void Initialize()
        {
            list = C("Main Menu List");
            functions["ArcadeMode"] = ArcadeMode;
            functions["CareerMode"] = CareerMode;
            functions["MultiPlayerMode"] = MultiPlayerMode;
            functions["OnMotocardAccept"] = () => { fe.SendUIEvent("GotoMotocardPage"); fe.PlaySound("Accept"); };
            functions["OnBackKeypress"] = () => { fe.SendUIEvent("Title Page"); fe.PlaySound("Back"); };
            functions["OnOptionsKeypress"] = () => { fe.SendUIEvent("GotoOptionsPage"); fe.PlaySound("Accept"); };
            functions["OnTutorialKeypress"] = () => FeTutorialScripts.OnTutorialKeypress(fe);   // MotoSkills (FrontEndTutorials.cs, WP-T)
            functions["OnDevMenuKeypress"] = () => { };     // RELEASE build: no dev menu
            list.onListChanged = FocusChanged;
        }
        public override void Start()
        {
            fe.SetUiMusicTier(2);
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("toggle", true, "STR_OPTIONS");
            fe.SetButtonLegend("misc", true, "STR_TUTORIALS");
            fe.SetButtonLegend("start", true, "STR_QUIT");
            fe.SetButtonLegend("dev", false, "STR_DEV_MENU");
            fe.legend.SetAction("start", () => fe.SendUIEvent("ShowQuitConfirmationPopup"));   // MouseActivation_Container: startContainer -> OnQuitKeypress
            fe.SetHeader("STR_MAINMENU");
            fe.vars["InCareerMode"] = false; fe.vars["PlayerMode"] = "None";
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && list.kids.Count > 1)
                SetGarageLabel(list.kids[1]);
            // the motocard item is not focusable without a signed-in profile (MXProfile:IsSignedIn: the profile session, FrontEndCareer.cs)
            if (list.kids.Count > 3) list.kids[3].focusable = SignedIn;
            if (!list.kids[list.focusIndex].focusable) list.focusIndex = 0;
        }
        public override void Present()
        {   // Refocused: the focused item plays its focused animation
            int i = list.focusIndex;
            if (i >= 0 && i < Data.Length) { layer.Stop(Data[i][1]); layer.Play(Data[i][0]); }
            FeTutorialScripts.MainMenuPresent(fe);   // PlayerRider.TutorialsEntered == FALSE -> TutorialFirstTimeEntryPopup (WP-T)
        }
        public override void Update(float dt) => FeTutorialScripts.Update(fe);   // the first-visit popup's START, after its save (WP-T)
        public override void End() { fe.legend.SetAction("start", () => fe.DispatchKey("Start")); }
        void FocusChanged(FeNode l, int old)
        {
            int i = l.focusIndex;
            if (old != i && old >= 0 && old < Data.Length) { layer.Stop(Data[old][0]); layer.Play(Data[old][1]); }
            if (i >= 0 && i < Data.Length) { layer.Stop(Data[i][1]); layer.Play(Data[i][0]); }
        }
        void ArcadeMode() { fe.vars["PlayerMode"] = "SinglePlayer"; fe.SendUIEvent("Arcade Mode Menu Page"); fe.PlaySound("Accept"); }
        static bool SignedIn => ReflexProfile.Current != null && ReflexProfile.Current.signedIn;
        // MainMenu:CareerMode: no profile -> the profile required popup; else PlayerMode SinglePlayer, EnableTourMode, InCareerMode and
        // GetPlayerEventStatus "CareerWelcome" ? CareerModeMenuPage : CareerWelcomePage.
        static void SetGarageLabel(FeNode node)
        {
            if (node.text) node.SetText("GARAGE");
            foreach (var child in node.kids) SetGarageLabel(child);
        }
        void CareerMode()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && SignedIn)
            {
                fe.vars["PlayerMode"] = "SinglePlayer"; fe.vars["InCareerMode"] = false;
                fe.JumpTo("Garage Page"); fe.PlaySound("Accept"); return;
            }
            if (!SignedIn) { fe.SendUIEvent("ShowProfileRequiredPopup"); return; }
            fe.vars["PlayerMode"] = "SinglePlayer"; fe.vars["InCareerMode"] = true;
            fe.SendUIEvent(ReflexProfile.Current.EventStatus("CareerWelcome") != 0 ? "CareerModeMenuPage" : "CareerWelcomePage");
            fe.PlaySound("Accept");
        }
        void MultiPlayerMode()
        {
            fe.PlaySound("Accept");
            fe.vars["NetworkNotification"] = "DisconnectedOnlineFeature";   // NetworkEnabled == FALSE
            fe.SendUIEvent("ShowNetworkNotificationPopup");
        }
    }

    // A dynamic list of poster slots (UIScrollingDynamicListContainer "Item0..5" in a TextureMask, EventSelect / LocationSelect):
    // the focused data item is drawn by the separate "FocusedItem" container in the slot at x = 0 (that slot is hidden), the
    // others to its sides; a focus change scrolls the row by one slot (TimeToScroll, EaseFunction Quintic).
    public sealed class FePosterList
    {
        public readonly FeNode list; public readonly List<FeNode> slots; public readonly int focusSlot; readonly float spacing, time;
        float t = 1, from;
        public FePosterList(FeNode list)
        {
            this.list = list;
            slots = list.kids.OrderBy(k => k.pos.x).ToList();
            focusSlot = slots.FindIndex(k => Mathf.Abs(k.pos.x) < 0.5f); if (focusSlot < 0) focusSlot = slots.Count / 2;
            spacing = slots.Count > 1 ? (slots[slots.Count - 1].pos.x - slots[0].pos.x) / (slots.Count - 1) : 200;
            time = OriginalUi.F(list.props, "TimeToScroll", 0.15f);
        }
        /// Calls format(slot, dataIndex, focused) for every visible slot; empty slots are hidden.
        public void Format(Action<FeNode, int> format)
        {
            for (int s = 0; s < slots.Count; s++)
            {
                int d = list.absIndex + (s - focusSlot);
                bool show = d >= 0 && d < list.numItems && s != focusSlot;
                slots[s].Visible = show;
                if (show) format(slots[s], d);
            }
        }
        public void Scrolled(int delta) { from = delta * spacing; t = 0; }
        public void Update(float dt)
        {
            if (t >= 1) { list.aPos.x = 0; return; }
            t = Mathf.Min(1, t + dt / Mathf.Max(0.01f, time));
            float e = 1 - Mathf.Pow(1 - t, 5);   // quintic ease-out
            list.aPos.x = from * (1 - e);
        }
    }

    // EventSelect.lua: the arcade event types (Free Ride, National, Supercross, Terracross, Freestyle, Powercross, Waypoint) as a
    // scrolling row of activity posters (EventType.IconId -> ActivityTypePosters), the info box (activity name, description,
    // vehicles), arrows; Accept -> the vehicle select page (FrontEndVehicleSelect.cs), Back -> main menu.
    public sealed class EventSelectScript : FeScript
    {
        sealed class Row { public int id = -1; public string activity, desc, vehicles; public int iconId = -1; public bool isNew; }
        List<Row> data; FePosterList posters; FeNode focusedItem, title, description, vehicleText, leftArrow, rightArrow;
        static readonly string[][] Types =
        {
            new[] { "Freeride", "STR_FREERIDE_DESCRIPTION", "STR_FREERIDE_VEHICLES" }, new[] { "National", "STR_NATIONAL_DESCRIPTION", "STR_NATIONAL_VEHICLES" },
            new[] { "Supercross", "STR_SUPERCROSS_DESCRIPTION", "STR_SUPERCROSS_VEHICLES" }, new[] { "Terracross", "STR_TERRACROSS_DESCRIPTION", "STR_TERRACROSS_VEHICLES" },
            new[] { "Freestyle", "STR_FREESTYLE_DESCRIPTION", "STR_FREESTYLE_VEHICLES" }, new[] { "Powercross", "STR_POWERCROSS_DESCRIPTION", "STR_POWERCROSS_VEHICLES" },
            new[] { "WaypointRace", "STR_WAYPOINT_DESCRIPTION", "STR_WAYPOINT_VEHICLES" },
        };
        public override void Initialize()
        {
            posters = new FePosterList(C("Event Select List"));
            focusedItem = C("FocusedItem"); title = C("Event Title"); description = C("Event Description"); vehicleText = C("Vehicle Text");
            leftArrow = C("Left Arrow"); rightArrow = C("Right Arrow");
            functions["OnAccept"] = OnAccept;
            functions["OnKeypressBack"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("GotoMainMenuPage"); };
            posters.list.onListChanged = (l, old) => { posters.Scrolled(l.absIndex - old); OnEventTypeListFocusedChanged(); };
            data = Types.Select(t =>
            {
                var r = fe.assets.tables.EventTypeOf(t[0]);
                return new Row { id = FeTables.I(r, "EventTypeId"), activity = FeTables.S(r, "ActivityName") ?? t[0], desc = t[1], vehicles = t[2], iconId = FeTables.I(r, "IconId") };
            }).ToList();
        }
        public override void Start()
        {
            fe.SetUiMusicTier(3);
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("toggle", false, "STR_RULES");
            fe.SetButtonLegend("misc", false, "STR_OPTIONS");
            posters.list.numItems = data.Count;
            posters.list.absIndex = Mathf.Clamp(posters.list.absIndex, 0, data.Count - 1);
            fe.SetHeader("STR_EVENT_SELECT");
            OnEventTypeListFocusedChanged();
        }
        public override void Update(float dt) => posters.Update(dt);
        public int Focused => posters.list.absIndex;
        public int Count => data.Count;

        void Format()
        {
            posters.Format((slot, d) => SetPoster(slot, d));
            SetPoster(focusedItem, posters.list.absIndex);
            var row = data[posters.list.absIndex];
            title.SetText(Str("STR_MXACTIVITYTYPE_" + row.activity.ToUpperInvariant()));   // App.GetMXActivityTypeLocalizationStringKey
            description.SetText(Str(row.desc));
            vehicleText.SetText(Str(row.vehicles));
        }
        void SetPoster(FeNode container, int d)
        {
            var img = container.Find("Event Image");
            if (img?.image) { var (tex, uv) = fe.assets.Icon(data[d].iconId); img.image.texture = tex; img.image.uvRect = uv; img.image.Dirty(); }
            if (container.Find("NewImage") is FeNode n) n.Visible = data[d].isNew;
        }
        void OnEventTypeListFocusedChanged()
        {
            layer.Stop("FocusedItem_Changed"); layer.Play("FocusedItem_Changed");
            Format();
            int i = posters.list.absIndex, n = posters.list.numItems;   // UpdateArrows
            leftArrow.Visible = i > 0; rightArrow.Visible = i < n - 1;
        }
        void OnAccept()
        {
            var row = data[posters.list.absIndex];
            fe.vars["EventType"] = row.activity; fe.vars["EventTypeId"] = row.id;
            fe.SendUIEvent("GotoVehicleSelectPage");
            fe.PlaySound("Accept");
        }
    }

    // LocationSelect.lua: the locations of the chosen event type (EventTypeLocations, posters PosterIconId; preview PreviewIconId /
    // track map MapIconId toggled by Misc), the rules box (Toggle focuses it): Event Duration (EventTypeDurations), AI Opponents
    // (None / Rookie .. All Time = AIDifficulty 0 / 80 / 95 / 105 / 120, default 95; on = 12 riders) and Vehicle Class (the classes
    // of the event type that contain the default vehicle's type). Accept on an unlocked location starts the event.
    public sealed class LocationSelectScript : FeScript
    {
        FePosterList posters; FeNode focusedItem, name, description, trackPreview, locationPreview, leftArrow, rightArrow, rules, rulesList, durationList, aiList, classList;
        List<FeTables.Location> data = new List<FeTables.Location>();
        List<int> durations; string singular, plural; List<(int id, string key, int types)> classes;
        bool rulesAvailable = true, rulesAccessible = true, resetPosition = true;
        static readonly (int skill, string key)[] AiSkills = { (0, "STR_AI_NONE"), (80, "STR_AI_LEVEL1"), (95, "STR_AI_LEVEL2"), (105, "STR_AI_LEVEL3"), (120, "STR_AI_LEVEL4") };

        public override void Initialize()
        {
            posters = new FePosterList(C("Location Select List"));
            focusedItem = C("FocusedItem"); name = C("Location Name"); description = C("Location Description");
            trackPreview = C("Track Preview"); locationPreview = C("Location Preview"); leftArrow = C("Left Arrow"); rightArrow = C("Right Arrow");
            rules = C("RulesContainer"); rulesList = C("RulesList"); durationList = C("EventDurationList"); aiList = C("AISkillList"); classList = C("VehicleClassList");
            functions["OnToggleRules"] = OnToggleRules;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); };
            functions["OnAccept"] = OnAccept;
            functions["OnToggleTrackPreview"] = OnToggleTrackPreview;
            posters.list.onListChanged = (l, old) => { posters.Scrolled(l.absIndex - old); EventLocationListFocusedChanged(); };
            durationList.onListChanged = (l, old) => OnEventDurationListFocusedChanged();
            aiList.onListChanged = (l, old) => OnAISkillListFocusedChanged();
            classList.onListChanged = (l, old) => OnVehicleClassListFocusedChanged();
        }

        public override void Start()
        {
            AllowRules(true);
            fe.SetUiMusicTier(5);
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("misc", true, "STR_LOCATION_PREVIEW");
            rules.Visible = rulesAvailable;
            fe.SetHeader("STR_LOCATION_SELECT");
            int typeId = fe.Var("EventTypeId", 10);
            data = fe.assets.tables.Locations(typeId);
            posters.list.numItems = data.Count;
            if (resetPosition) posters.list.absIndex = 0;
            posters.list.absIndex = Mathf.Clamp(posters.list.absIndex, 0, Math.Max(0, data.Count - 1));
            resetPosition = true;
            OnEnableRules(false);
            locationPreview.Visible = true; trackPreview.Visible = false;
            if (rulesAvailable) { BuildDurationList(); BuildAISkillList(); BuildVehicleClassList(); }
            else { fe.vars["AIDifficulty"] = 0; fe.vars["EventRiders"] = 1; fe.vars["VehicleClass"] = -1; }
            EventLocationListFocusedChanged();
        }
        public override void Update(float dt) => posters.Update(dt);
        public IReadOnlyList<FeTables.Location> Locations => data;
        public int Focused => posters.list.absIndex;
        public int AiIndex => aiList.absIndex;

        // ---------------------------------------------------------------------------------------- lists
        void BuildDurationList()
        {
            durations = fe.assets.tables.Durations(fe.Var("EventTypeId", 10), out singular, out plural, out string type) ?? new List<int> { 0 };
            fe.vars["EventDurationType"] = type;
            durationList.numItems = durations.Count;
            durationList.absIndex = durations.Count > 1 ? 1 : 0;
            OnEventDurationListFocusedChanged();
        }
        void BuildAISkillList()
        {
            aiList.numItems = AiSkills.Length; aiList.absIndex = 2;
            OnAISkillListFocusedChanged();
        }
        void BuildVehicleClassList()
        {
            classes = fe.assets.tables.VehicleClasses(fe.Var("EventTypeId", 10), fe.Var("Player1VehicleTypeId", fe.assets.tables.DefaultVehicleTypeId()));   // VehicleSelect's type
            classList.numItems = classes.Count; classList.absIndex = 0;
            OnVehicleClassListFocusedChanged();
        }

        static FeNode RowText(FeNode list) => list.kids.Count > 0 ? list.kids[0].Find("Text") : null;
        void OnEventDurationListFocusedChanged()
        {
            int v = durations != null && durationList.absIndex < durations.Count ? durations[durationList.absIndex] : 0;
            fe.vars["EventDuration"] = v;
            // EMB_EVENT_DURATION_0 = DYN_EVENT_DURATION_0 + " " + DYN_EVENT_DURATION_LABEL_0 (plural key when > 1)
            RowText(durationList)?.SetText(v + " " + Str(v > 1 ? plural : singular));
            UpdateArrows(durationList, durationList.parent.Kid("LeftArrow"), durationList.parent.Kid("RightArrow"));
        }
        void OnAISkillListFocusedChanged()
        {
            var a = AiSkills[Mathf.Clamp(aiList.absIndex, 0, AiSkills.Length - 1)];
            fe.vars["AIDifficulty"] = a.skill;
            fe.vars["EventRiders"] = a.skill == 0 ? 1 : 12;
            RowText(aiList)?.SetText(Str(a.key));
            UpdateArrows(aiList, aiList.parent.Kid("LeftArrow"), aiList.parent.Kid("RightArrow"));
        }
        void OnVehicleClassListFocusedChanged()
        {
            if (classes == null || classes.Count == 0) { RowText(classList)?.SetText(""); return; }
            var c = classes[Mathf.Clamp(classList.absIndex, 0, classes.Count - 1)];
            fe.vars["VehicleClass"] = c.id;
            RowText(classList)?.SetText(Str(c.key));
            UpdateArrows(classList, classList.parent.Kid("LeftArrow"), classList.parent.Kid("RightArrow"));
        }

        void FormatLocations()
        {
            posters.Format((slot, d) => SetPoster(slot, d));
            if (data.Count == 0) { focusedItem.Visible = false; name.SetText(""); description.SetText(""); return; }
            focusedItem.Visible = true;
            int i = posters.list.absIndex; var l = data[i];
            SetPoster(focusedItem, i);
            name.SetText(Str(fe.assets.tables.TitleKey(l.storeProductId)));
            description.SetText(l.descriptionLocalizationId != -1 ? Str(fe.assets.tables.LocalizationKey(l.descriptionLocalizationId)) : "");
            SetIcon(locationPreview, l.iconId);
            SetIcon(trackPreview, l.mapIconId);
        }
        void SetIcon(FeNode n, int icon)
        {
            if (n?.image == null) return;
            var (tex, uv) = fe.assets.Icon(icon); n.image.texture = tex; n.image.uvRect = uv; n.image.Dirty();
        }
        void SetPoster(FeNode container, int d)
        {
            var l = data[d];
            SetIcon(container.Find("Location Image"), l.posterIconId);
            if (container.Find("LockImage") is FeNode lk) lk.Visible = l.locked;
            if (container.Find("NewImage") is FeNode nw) nw.Visible = l.isNew;
            if (container.Find("DLCImage") is FeNode dl) dl.Visible = l.dlc;
        }

        void EventLocationListFocusedChanged()
        {
            if (data.Count > 0) AllowRules(!data[posters.list.absIndex].locked);
            layer.Stop("FocusedItem_Changed"); layer.Play("FocusedItem_Changed");
            FormatLocations();
            if (!rules.focusedFlag) UpdateArrows(posters.list, leftArrow, rightArrow);
        }

        static void UpdateArrows(FeNode list, FeNode left, FeNode right)
        {
            if (left == null || right == null) return;
            int i = list.absIndex, n = list.numItems;
            if (n <= 1) { left.Visible = false; right.Visible = false; }
            else { left.Visible = i > 0; right.Visible = i < n - 1; }
        }

        // ---------------------------------------------------------------------------------------- actions
        void OnAccept()
        {
            if (data.Count == 0) return;
            var l = data[posters.list.absIndex];
            if (l.locked) return;
            fe.vars["EventLocation"] = l.filename; fe.vars["EventLocationId"] = l.eventLocationId; fe.vars["TrackId"] = l.trackId;
            resetPosition = false;
            fe.PlaySound("Accept");
            // App.SetupEvent / App.StartEvent / GotoLoadingPage: the race scene with this event; AI as ReflexEventSelect's choice
            // (0 off, 1..4 = ReflexWorldFiles.AiSkills Rookie 80 / Amateur 95 / Pro 105 / All Time 120 = STR_AI_LEVEL1..4).
            int ai = rulesAvailable ? Mathf.Clamp(aiList.absIndex, 0, AiSkills.Length - 1) : 0;
            // Event Duration (EventTypeDurations: laps, Freestyle's minutes) goes into the event like App.SetUIVariable
            // "EventDuration" before SetupEvent (OnEventDurationListFocusedChanged sets it for every type); types without a list keep the table's.
            int laps = rulesAvailable ? fe.Var("EventDuration", 0) : 0;
            fe.LaunchEvent(l.ev, ai, laps, LaunchGarage(rulesAvailable ? fe.Var("VehicleClass", -1) : -1));
        }

        /// The Vehicle Select choice (VehicleSelect:End) and the chosen VehicleClass for the race's world. A choice that equips what the
        /// profile already has (the player's own rider on PlayerRider.LastEquippedVehicle, LastRiderId 0) is left to EnsureLocalPlayers,
        /// so the default race keeps its world (the Equip calls would only renumber the session tables).
        ReflexWorldFiles.Garage LaunchGarage(int vehicleClass)
        {
            var chosen = fe.Var<ReflexWorldFiles.Garage>("Garage");
            var g = chosen == null ? new ReflexWorldFiles.Garage() : new ReflexWorldFiles.Garage
            {
                riderOwner = chosen.riderOwner, riderId = chosen.riderId, riderGearSet = chosen.riderGearSet,
                vehicleOwner = chosen.vehicleOwner, vehicleId = chosen.vehicleId, vehicleTypeId = chosen.vehicleTypeId,
            };
            var pr = fe.assets.tables.Rows("PlayerRider").FirstOrDefault();
            if (g.riderOwner == ReflexWorldFiles.Garage.Player1 && g.vehicleOwner == ReflexWorldFiles.Garage.Player1 && FeTables.I(pr, "LastRiderId", 0) < 1 && g.vehicleId == FeTables.I(pr, "LastEquippedVehicle", 0))
                g.riderOwner = g.vehicleOwner = -1;
            g.vehicleClass = vehicleClass;
            return g.Equips || vehicleClass >= 0 ? g : null;
        }

        void OnToggleRules()
        {
            if (rulesAccessible && rulesAvailable)
            {
                fe.PlaySound("Accept");
                OnEnableRules(!rules.focusedFlag);
            }
        }
        void OnEnableRules(bool enable)
        {
            if (enable)
            {
                posters.list.focusedFlag = false; rules.focusedFlag = true;
                leftArrow.Visible = false; rightArrow.Visible = false;
                fe.SetButtonLegend("toggle", true, "STR_LOCATION");
            }
            else
            {
                posters.list.focusedFlag = true; rules.focusedFlag = false;
                fe.SetButtonLegend("toggle", rulesAccessible, "STR_RULES");
                UpdateArrows(posters.list, leftArrow, rightArrow);
            }
        }
        void AllowRules(bool allow)
        {
            if (fe.Var("EventType", "") == "FreeRide") { rulesAvailable = false; rulesAccessible = false; }
            else { rulesAvailable = true; rulesAccessible = allow; }
            fe.SetButtonLegend("toggle", rulesAccessible, "STR_RULES");
        }
        void OnToggleTrackPreview()
        {
            fe.PlaySound("Accept");
            bool track = trackPreview.visible;
            trackPreview.Visible = !track; locationPreview.Visible = track;
        }
    }

    // GenericOneButtonPopup layers (ProfileReminderPopup.lua / ProfileRequiredPopup.lua / NetworkNotificationPopup.lua): title,
    // message and button text by key; Accept / Back pop the page stack (the reminder's End then goes to the main menu when accepted).
    public sealed class OneButtonPopupScript : FeScript
    {
        readonly string title, message, button; readonly Action<FrontEndManager> onAccepted; bool accepted;
        public OneButtonPopupScript(string title, string message, string button, Action<FrontEndManager> onAccepted) { this.title = title; this.message = message; this.button = button; this.onAccepted = onAccepted; }
        public override void Initialize()
        {
            functions["OnAcceptKeypress"] = () => { accepted = true; fe.SendUIEvent("PopupPageStack_Pop"); fe.PlaySound("Accept"); };
            functions["OnBackKeypress"] = () => { fe.SendUIEvent("PopupPageStack_Pop"); fe.PlaySound("Back"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            accepted = false;
            C("TitleText")?.SetText(Str(title));
            var m = C("MessageText"); m?.SetText(Str(message));
            C("ButtonText")?.SetText(Str(button));
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End() { if (accepted) onAccepted?.Invoke(fe); }
    }

    // QuitConfirmationPopup.lua: QUIT / "Are you sure you want to quit your game?..." / YES (focused) / NO; YES quits the game.
    public sealed class QuitConfirmationPopupScript : FeScript
    {
        public override void Initialize()
        {
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); fe.QuitGame(); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("HideQuitConfirmationPopup"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("HideQuitConfirmationPopup"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            C("TitleText")?.SetText(Str("STR_QUIT"));
            C("MessageText")?.SetText(Str("STR_QUITGAMEPOPUP_MESSAGE"));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str("STR_YES"));
            C("ButtonTwo")?.Find("ButtonText")?.SetText(Str("STR_NO"));
            var list = C("ButtonList"); if (list != null) list.focusIndex = 0;      // ButtonOne's container SetFocused( TRUE )
            KeyGlyph.Refresh(fe, layer.root);
        }
    }

    // UIImageComponent KeyMap=MapKey_UI_*: on the PC the original's image is UIEmpty (nothing; OriginalInput.cs ReflexKeyMapGlyphs, WP-C).
    // Port extra with ReflexKeyHints 1: the button glyph of the device (Xbox 360 ControllerButtons sprite with a gamepad; with the
    // keyboard the key KeyBind_Core.xml binds, in the game's font, as the button legend does).
    public static class KeyGlyph
    {
        static readonly Dictionary<string, string> Pad = new Dictionary<string, string>
        { ["Accept"] = "Xenon_Controller_Button_1", ["Back"] = "Xenon_Controller_Button_2", ["Toggle"] = "Xenon_Controller_Button_3", ["Misc"] = "Xenon_Controller_Button_4", ["Start"] = "Xenon_Controller_Button_7", ["Select"] = "Xenon_Controller_Button_8" };
        static readonly Dictionary<string, string> Legend = new Dictionary<string, string>
        { ["Accept"] = "accept", ["Back"] = "back", ["Toggle"] = "toggle", ["Misc"] = "misc", ["Start"] = "start", ["Select"] = "dev" };

        public static void Refresh(FrontEndManager fe, FeNode n)
        {
            if (n.keyMap != null && n.image && !ReflexKeyMapGlyphs.PortHints) { n.hideArt = true; if (n.keyText) n.keyText.Text = ""; }   // the PC game's UIEmpty (OriginalInput.cs ReflexKeyMapGlyphs)
            else if (n.keyMap != null && n.image)
            {
                string action = n.keyMap.Replace("MapKey_UI_", "");
                bool pad = UnityEngine.InputSystem.Gamepad.current != null;
                if (pad && Pad.TryGetValue(action, out var sn) && fe.assets.data.Atlas("BaseLocalized", "ControllerButtons").TryGetValue(sn, out var sp))
                {
                    n.image.texture = OriginalUiAssets.Texture(fe.assets.data.TextureKeyOf("BaseLocalized", sp.texture));
                    n.image.uvRect = new Rect(sp.u0, 1f - sp.v1, sp.u1 - sp.u0, sp.v1 - sp.v0); n.image.Dirty();
                    if (n.keyText) n.keyText.Text = "";
                    n.hideArt = false;
                }
                else
                {
                    n.hideArt = true;   // the key image of the keyboard: its name
                    if (n.keyText)
                    {   // the key name, fitted into the image's box
                        var kt = n.keyText; string key = Legend.TryGetValue(action, out var b) ? OriginalButtonLegend.KeyName(b) : action.ToUpperInvariant();
                        float h = Mathf.Min(24f, n.size.y * 0.75f); kt.textHeight = h; kt.Text = key;
                        float w = kt.LineWidth(key); if (w > n.size.x && w > 0) kt.textHeight = h * n.size.x / w;
                        kt.Resize();
                    }
                }
            }
            foreach (var k in n.kids) Refresh(fe, k);
        }
    }
}

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The in-race pages around the race (WP-5, _re/notes/in-race-pages.md; native/drafts/world_runtime/pages.cpp), read from the running game
    // between its frames ("state" every 0.2 s):
    //   Event Intro Page   EventIntro.lyr (EventIntro.lua): the event's title (TitleLocalizationString of the EventTypeLocations row's
    //                      StoreProductId) over its location (EventLocation.LocalizationId; hidden in Free Ride), ScenarioHeaderIn as the page
    //                      starts, while the intro scenario plays (race state Pre, after the rules page) until the HUD page (the countdown);
    //                      accept (Enter / A) = SkipToNextScenario. Worlds whose UI runs live (OriginalLiveUi: the modes and the arcade races)
    //                      show the game's own page; this draws it for the others (a MOTOCAREER race) with the texts the page's own Lua
    //                      computes, run in the world.
    //   Event Rider Page   EventRiderName.lyr: nothing on PC sends "GotoEventRiderPage" (no such string in MXReflex.exe, no Lua): unreachable.
    //   Event Outro Page   EventOutro (an empty layer; EventOutro.lua: AllowPause( FALSE ), accept = SkipToNextScenario): the post scenarios
    //                      after the finish. MXActivityRace_UpdatePost (00569a90, A+0x13e640: 1 while the post scenarios play) - the
    //                      post-race pages (OriginalPostRace / OriginalCareerRace) wait for their end ("Post Scenarios End" -> GotoPostEvent)
    //                      as the game's PostEventOverlay does (UIRoutingMessage GotoEventOutroPage at state 4, GotoPostEvent ~5.3 s later).
    //   pause              App.AllowPause (the UI manager's +0x28c, read by MXUIManager_OnPauseRequest 00598e40): FALSE on the Event Intro /
    //                      Outro pages, TRUE from the HUD page. With the live UI the game's own flag; otherwise the same pages' rule.
    //   voice              App.PlayUILocalizedSoundEvent / StopUILocalizedSoundEvent of the page Lua run in the world (RulesPopup:Start's rules
    //                      narration, AAANotification's CHALLENGE_APPEAR, ...; reflex_world_ui_voice) on the race's event player (ReflexUiVoice).
    // Also the pause page's data (OriginalPauseMenu): its rows as PauseMenu:Start decides them and the trick library (StuntPreview), both
    // from the game's own Lua in the world.
    [DisallowMultipleComponent, DefaultExecutionOrder(-50)]   // before OriginalLiveUi: the hooks go in before the live UI's first frame
    public sealed class OriginalEventIntro : MonoBehaviour
    {
        // ------------------------------------------------------------------------------------------------- native (pages.cpp)
        const string Lib = ReflexWorldNative.Library;
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_pages__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_pages__fn> reflex_world_pages__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_pages__fn>("reflex_world_pages");
        static int reflex_world_pages(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string command, byte[] output, int size) => reflex_world_pages__p.Get()(world, command, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_voice__fn(IntPtr world, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_voice__fn> reflex_world_ui_voice__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_voice__fn>("reflex_world_ui_voice");
        static int reflex_world_ui_voice(IntPtr world, byte[] output, int size) => reflex_world_ui_voice__p.Get()(world, output, size);
        static bool missing;   // an older plugin (no pages.cpp): the pages fall back to the port's own rules
        public static bool NativeMissing => missing;

        /// A pages command on the world (between its frames, on the simulation thread); null on error or with an older plugin.
        public static string Command(IntPtr world, string command)
        {
            if (world == IntPtr.Zero || missing) return null;
            try
            {
                var buf = new byte[1 << 15];
                int n = reflex_world_pages(world, command, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_world_pages(world, command, buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Pages] " + command.Split(' ')[0] + ": " + ReflexWorldNative.Error(world, "failed")); return null; }
                return Encoding.UTF8.GetString(buf, 0, n);
            }
            catch (EntryPointNotFoundException) { missing = true; Debug.Log("[Pages] the plugin has no reflex_world_pages (older libreflexworld): the port's own page rules"); return null; }
            catch (Exception e) { Debug.LogWarning("[Pages] " + e.Message); return null; }
        }
        /// The game's UI Lua run in the world (pages.cpp "lua"); its answer is what it passes to App.DebugPrint( "REFLEXANSWER:" .. answer ).
        public static string Lua(IntPtr world, string source) => Command(world, "lua " + source);
        static string Voice(IntPtr world)
        {
            if (world == IntPtr.Zero || missing) return null;
            try { var b = new byte[4096]; int n = reflex_world_ui_voice(world, b, b.Length); return n > 0 ? Encoding.UTF8.GetString(b, 0, n) : null; }
            catch (EntryPointNotFoundException) { missing = true; return null; }
            catch (Exception e) { Debug.LogWarning("[Pages] voice: " + e.Message); return null; }
        }

        /// The runtime's pages component (added on first use).
        public static OriginalEventIntro For(Component c)
        {
            if (!c) return null;
            var rt = c.GetComponent<OriginalWorldRuntime>() ?? c.GetComponentInParent<OriginalWorldRuntime>() ?? FindAnyObjectByType<OriginalWorldRuntime>();
            if (!rt) return null;
            return rt.GetComponent<OriginalEventIntro>() ?? rt.gameObject.AddComponent<OriginalEventIntro>();
        }

        // ------------------------------------------------------------------------------------------------- the world's state
        public struct State { public int race, rulesHold, allowPause, post, skip; public uint vtable; public bool valid; }
        OriginalWorldRuntime runtime;
        readonly object gate = new object();
        State state, pendingState; bool statePending, asking; float askTimer;
        OriginalSimulation simSeen;   // the world (its simulation) the cached data belongs to
        bool dataAsked; string pendingRows, pendingStunts, pendingIntro; bool rowsReady, stuntsReady, introReady;
        public State Current => state;
        public bool HasState => state.valid;
        /// The post scenarios play (the Event Outro page): race state Post, MXActivityRace's post sub-state 1 (or 0 the first frame of Post).
        public bool OutroRunning => state.valid && state.race == 4 && (state.post == 0 || state.post == 1);
        /// The world's UI runs live (OriginalLiveUi): its own Event Intro / Outro pages and pause flag.
        public bool LiveUi => runtime && runtime.TryGetComponent<OriginalLiveUi>(out var l) && l.enabled && l.Running;

        /// MXUIManager_OnPauseRequest's App.AllowPause: the game's own flag when its UI runs live, else the same pages' rule (no pause on the
        /// Event Intro page - race state Pre once the rules page is done - nor during the post scenarios). True without a state.
        public bool PauseAllowed
        {
            get
            {
                if (!state.valid) return true;
                if (LiveUi) return state.allowPause != 0;
                if (state.race == 0 && state.rulesHold == 0) return false;   // Event Intro
                if (OutroRunning) return false;                                // Event Outro
                return true;
            }
        }

        void Awake() { runtime = GetComponent<OriginalWorldRuntime>(); }

        void Update()
        {
            if (!runtime) return;
            var sim = runtime.Simulation;
            if (!runtime.Running || sim == null) { HideIntro(); return; }
            // a new world (create / restart): its pages start again
            if (sim != simSeen) { simSeen = sim; ResetWorld(); }
            if (!dataAsked) { dataAsked = true; AskData(sim); }
            askTimer -= Time.unscaledDeltaTime;
            bool ask; lock (gate) { ask = !asking && askTimer <= 0; if (ask) asking = true; }
            if (ask)
            {
                askTimer = 0.2f;
                sim.BetweenFrames(w =>
                {
                    string s = Command(w, "state"), v = Voice(w), img = Command(w, "ui_images");
                    if (!string.IsNullOrEmpty(img)) lock (gate) imageLines.Append(img);
                    var st = Parse(s);
                    int[] now = null;
                    if (!missing) { now = new int[SyncedSettings.Length]; for (int i = 0; i < now.Length; i++) now[i] = UiSetting(w, SyncedSettings[i], 0, 0); }
                    bool retry; lock (gate) retry = !quickViewPatched && patchRetry <= 0;
                    if (retry) { bool ok = Lua(w, QuickViewPatchLua) == "1"; lock (gate) { quickViewPatched = ok; patchRetry = 1f; } }
                    lock (gate) settingPending = now;
                    lock (gate) { if (st.valid) { pendingState = st; statePending = true; } asking = false; if (v != null) voiceLines.Append(v); }
                });
            }
            string voice = null; int[] settingsNow = null;
            lock (gate)
            {
                patchRetry -= Time.unscaledDeltaTime;
                settingsNow = settingPending; settingPending = null;
                if (imageLines.Length > 0) { ApplyImages(imageLines.ToString()); imageLines.Clear(); }
                if (statePending) { state = pendingState; statePending = false; }
                if (voiceLines.Length > 0) { voice = voiceLines.ToString(); voiceLines.Clear(); }
                if (pendingRows != null) { ApplyRows(pendingRows); pendingRows = null; }
                if (pendingStunts != null) { ApplyStunts(pendingStunts); pendingStunts = null; }
                if (pendingIntro != null) { ApplyIntroTexts(pendingIntro); pendingIntro = null; }
            }
            if (voice != null) PlayVoice(voice);
            // A world whose live UI put the RulesPopup up before the hooks were in (its Start already asked for the narration): the
            // narration RulesPopup:Start plays for the RulesType, under the layer's own owner name (its End's stop ends it)
            string rta; lock (gate) { rta = rulesTypeAtStart; rulesTypeAtStart = null; if (rulesTypeFresh) { rulesTypeFresh = false; rulesTypeUntil = Time.unscaledTime + 5f; } }
            if (rta != null && rta.Length > 0 && LiveUi && runtime.TryGetComponent<OriginalLiveUi>(out var lv) && lv.Shows("RaceRulesPopup") && !voiceWrappers.ContainsKey("RulesPopup.lyr"))
            {
                var rv = ReflexUiVoice.RulesVoice(rta);
                if (rv.HasValue) PlayVoice("play " + rv.Value.db + "|" + rv.Value.package + "|" + rv.Value.eventPath + "|RulesPopup.lyr\n");
            }
            else if (rta != null && rta.Length > 0 && !LiveUi && Time.unscaledTime < rulesTypeUntil) lock (gate) rulesTypeAtStart ??= rta;   // the live UI's first frame is not in yet
            if (settingsNow != null)
                for (int i = 0; i < settingsNow.Length; i++)
                {
                    int v = settingsNow[i]; if (v < 0) continue;
                    if (settingSeen[i] >= 0 && v != settingSeen[i]) ApplySetting(i, v);   // changed in the world (the game's PauseOptions)
                    settingSeen[i] = v;
                }
            TickIntro();
            TickOutro();
        }

        void ResetWorld()
        {
            lock (gate) { state = default; statePending = false; pendingRows = pendingStunts = pendingIntro = null; asking = false; }
            dataAsked = false; askTimer = 0; rowsReady = stuntsReady = stuntsAnswered = introReady = false; needsFreeRideMap = false; freeRideMapIcon = -1; introShown = false; introDone = false;
            pauseRows.Clear(); challengeTitle = challengeDescription = null; HideIntro();
            codeImages.Clear(); stateColours.Clear(); quickViewPatched = false; patchRetry = 0; for (int i = 0; i < settingSeen.Length; i++) settingSeen[i] = -1;
            StopAllVoice();
        }

        static State Parse(string json)
        {
            var o = json == null ? null : OriginalUi.ParseJson(json) as Dictionary<string, object>;
            if (o == null) return default;
            return new State { valid = true, race = (int)OriginalUi.F(o, "race", -1), rulesHold = (int)OriginalUi.F(o, "rulesHold"), allowPause = (int)OriginalUi.F(o, "allowPause"),
                               post = (int)OriginalUi.F(o, "post", -1), skip = (int)OriginalUi.F(o, "skip"), vtable = (uint)OriginalUi.F(o, "vtable") };
        }

        /// SkipToNextScenario (EventIntro / EventOutro accept): [0xa7f748] = 1 in the world.
        public void SkipScenario()
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim != null) sim.BetweenFrames(w => Command(w, "skip_scenario"));
        }

        // ------------------------------------------------------------------------------------------------- pause data (PauseMenu:Start, StuntPreview)
        // The rows PauseMenu:Start builds (single player, PC: no demo, no network): its own conditions on the UI variables and the equipped
        // vehicle's owner, as one chunk of the game's Lua. Answer: the row keys, then the ChallengeEventContainer's title / description
        // keys (an activated challenge), then needsChallengeList / needsFreeRideMap.
        const string PauseRowsLua =
            "local needsOptions = TRUE; local needsVehicleSelect = FALSE; local needsResume = TRUE; local needsQuit = TRUE; local needsRestart = FALSE; " +
            "local needsChallengeList = FALSE; local needsChallengeEvent = FALSE; local needsFreeRideMap = FALSE; local needsNetworkPlayerList = FALSE; " +
            "local needsFriendsList = FALSE; local needsFriendsInvite = FALSE; local needsVehicleTuning = TRUE; local needsStunts = TRUE; " +
            "local eventType = App.GetUIVariableString( \"EventType\" ); local playerMode = App.GetUIVariableString( \"PlayerMode\" ); " +
            "local isAAA = App.GetUIVariableInt( \"EventTypeActivated\" ); local inCareerMode = App.GetUIVariableInt( \"InCareerMode\" ); " +
            "local inRealWorld = App.GetUIVariableInt( \"InRealWorld\" ); local isSpectator = App.GetUIVariableInt( \"SpectatorMode\" ); " +
            "local isWorldLoading = App.GetUIVariableInt( \"WorldLoading\" ); " +
            "if( eventType == \"FreeRide\" ) then needsVehicleSelect = TRUE; needsRestart = FALSE; needsChallengeList = inCareerMode; needsFreeRideMap = TRUE; needsVehicleTuning = TRUE; " +
            "else needsRestart = TRUE; needsVehicleTuning = FALSE; end " +
            "if( inRealWorld == FALSE ) then needsVehicleTuning = TRUE; end " +
            "if( playerMode == \"Online\" or playerMode == \"Lan\" ) then needsRestart = FALSE; needsChallengeList = FALSE; needsNetworkPlayerList = TRUE; if (inRealWorld == FALSE) then needsVehicleSelect = FALSE; end end " +
            "if ( playerMode == \"Online\" ) then needsFriendsInvite = TRUE; end " +
            "if( playerMode == \"Splitscreen\" ) then needsChallengeList = FALSE; end " +
            "if( isAAA == TRUE ) then needsVehicleSelect = FALSE; needsRestart = TRUE; needsChallengeList = FALSE; needsFreeRideMap = FALSE; needsChallengeEvent = TRUE; end " +
            "if ( isSpectator == TRUE ) then needsVehicleSelect = FALSE; end " +
            "if( isWorldLoading == TRUE ) then needsRestart = FALSE; needsChallengeList = FALSE; needsFreeRideMap = FALSE; needsChallengeEvent = FALSE; needsOptions = FALSE; needsVehicleSelect = FALSE; end " +
            "if( needsVehicleTuning == TRUE ) then " +
            "  local ok = pcall( function( ) " +
            "    local exclusiveDeviceIndex = App.GetExclusiveDeviceIndex(); if( exclusiveDeviceIndex < 0 ) then exclusiveDeviceIndex = App.GetPrimaryPlayerDeviceIndex( ); end " +
            "    local playerVehicleOwner = App.GetMXVehicleOwner( \"Player\" .. ( exclusiveDeviceIndex + 1 ) ); local mxTableHelper = App.GetMXTableHelper( ); " +
            "    local playerRow = mxTableHelper:GetPlayerTableRow( exclusiveDeviceIndex ); " +
            "    local vehicleResult, vehicleId, vehicleOwner = mxTableHelper:GetEquippedVehicle( playerRow:rowIndex( ) ); " +
            "    if( vehicleOwner ~= playerVehicleOwner ) then needsVehicleTuning = FALSE; end end ); " +
            "  if not ok then needsVehicleTuning = FALSE; end " +
            "end " +
            "local o = \"\"; " +
            "if( needsResume == TRUE ) then o = o .. \"STR_RESUME_GAME,\"; end " +
            "if( needsRestart == TRUE ) then o = o .. \"STR_RESTART_GAME,\"; end " +
            "if( needsNetworkPlayerList == TRUE ) then o = o .. \"STR_PLAYERS,\"; end " +
            "if( needsVehicleTuning == TRUE ) then o = o .. \"STR_GARAGE_VEHICLE_TUNE,\"; end " +
            "if( needsVehicleSelect == TRUE ) then o = o .. \"STR_QUICKVIEW,\"; end " +
            "if( needsStunts == TRUE ) then o = o .. \"STR_STUNTS,\"; end " +
            "if( needsOptions == TRUE ) then o = o .. \"STR_OPTIONS,\"; end " +
            "if( needsQuit == TRUE ) then o = o .. \"STR_QUIT,\"; end " +
            "local t = \"\"; local d = \"\"; " +
            "if( needsChallengeEvent == TRUE ) then local a = string.upper( eventType ); if( a == \"TRAILBLAZER\" ) then a = \"TRAILBLAZER\"; end t = \"STR_\" .. a .. \"_TITLE\"; d = \"STR_\" .. a .. \"_DESCRIPTION\"; end " +
            "local icon = -1; if( needsFreeRideMap == TRUE ) then local q = App.RdbQuery( ); q:AddOperation_int( \"EventTypeId\", App.Equals, 9 ); " +
            "q:AddOperation_int( \"EventLocationId\", App.Equals, App.GetUIVariableInt( \"EventLocationId\" ) ); local rs = App.Lookup( \"EventTypeLocations\" ):query( q ); " +
            "if( rs:GetSize( ) > 0 ) then rs:EnumReset( ); icon = rs:EnumNext( ):cell_int( \"HUDMapIconId\" ); end end " +
            "App.DebugPrint( \"REFLEXANSWER:\" .. o .. \"|\" .. t .. \"|\" .. d .. \"|\" .. tostring( needsChallengeList ) .. \"|\" .. tostring( needsFreeRideMap ) .. \"|\" .. tostring( icon ) )";
        // StuntPreview:PopulateStuntList's source of rows: every stunt of the four tiers (tier, StuntDescriptor name, display-name key).
        const string StuntsLua =
            "local o = \"\"; for stuntTier = 1, 4 do local c = App.GetStuntDescriptorTierContainer( stuntTier ); " +
            "for i = 0, c:GetNumStuntDescriptors( ) - 1 do o = o .. stuntTier .. \"|\" .. c:GetStuntDescriptorName( i ) .. \"|\" .. c:GetStuntDescriptorDisplayNameLocalization( i ) .. \"^\" end end; " +
            "App.DebugPrint( \"REFLEXANSWER:\" .. o )";
        // EventIntro:Start's texts as string-table keys: the location (EventLocation.LocalizationId; MXTableHelper:GetLocalizationString) and the
        // event (the EventTypeLocations row of EventTypeId / EventLocationId / TrackId -> TitleLocalizationString( StoreProductId ): StoreProducts
        // .LocalizationId), each through the Localization table's LocalizationKey; FreeRide hides the location. (The strings themselves are
        // wide-string userdata in the game's Lua: the keys are looked up here, in the install's string tables.)
        const string IntroLua =
            "local function key( id ) if( id == nil or id == -1 ) then return \"\" end local r = App.Lookup( \"Localization\" ):searchUnique_int( \"LocalizationId\", id ); " +
            "if( r:isNull( ) == FALSE ) then return r:cell_string( \"LocalizationKey\" ) end return \"\" end " +
            "local mxTableHelper = App.GetMXTableHelper( ); local loc = \"\"; local ev = \"\"; " +
            "local eventLocationId = App.GetUIVariableInt( \"EventLocationId\" ); " +
            "local row = App.Lookup( \"EventLocation\" ):searchUnique_int( \"EventLocationId\", eventLocationId ); " +
            "if( row:isNull( ) == FALSE ) then loc = key( row:cell_int( \"LocalizationId\" ) ); end " +
            "local trackId = App.GetUIVariableInt( \"TrackId\" ); local eventTypeString = App.GetUIVariableString( \"EventType\" ); " +
            "local eventTypeId = mxTableHelper:MapEventTypeId( App.GetMXActivityType( eventTypeString ) ); " +
            "local query = App.RdbQuery( ); query:AddOperation_int( \"EventTypeId\", App.Equals, eventTypeId ); query:AddOperation_int( \"EventLocationId\", App.Equals, eventLocationId ); " +
            "query:AddOperation_int( \"TrackId\", App.Equals, trackId ); local rows = App.Lookup( \"EventTypeLocations\" ):query( query ); rows:EnumReset( ); " +
            "if not ( rows:EnumDone( ) ) then local r = rows:EnumNext( ); if( r ~= nil and r:isNull( ) == FALSE ) then " +
            "local sp = App.Lookup( \"StoreProducts\" ):searchUnique_int( \"StoreProductId\", r:cell_int( \"StoreProductId\" ) ); if( sp:isNull( ) == FALSE ) then ev = key( sp:cell_int( \"LocalizationId\" ) ); end end end " +
            "local fr = 0; if( eventTypeString == \"FreeRide\" ) then fr = 1; end " +
            "App.DebugPrint( \"REFLEXANSWER:\" .. ev .. \"|\" .. loc .. \"|\" .. fr )";

        // The pause page's VEHICLE LIST in a live world (the game's own VehiclesQuickViewPopup): PauseMenu:OnQuickViewPlayerVehicleChanged /
        // OnQuickViewAIVehicleChanged equip the choice, SyncDatabaseWithManagers and EnableWorld - the vehicle's load, which the world
        // runtime cannot do (D3D CreateTexture on the null device stops the world) - then GotoVehicleLoadingPage. Here they leave the
        // choice in UI "ReflexQuickView" ("p|<PlayerVehicleId>" or "a|<AIVehicleId>|<AIRiderId>|<DefaultRiderGearSetId>") and send
        // GotoVehicleLoadingPage (held back: OriginalLiveUi -> OriginalQuickView.Reload builds the Free Ride world with that vehicle, the
        // equip done by the boot as VehicleSelect's). Answer 1 once patched (the PauseMenu script loaded).
        const string QuickViewPatchLua =
            "local ok = 0; if( PauseMenu ~= nil ) then ok = 1; " +
            "PauseMenu.OnQuickViewPlayerVehicleChanged = function( self ) local id = App.GetUIVariableInt( \"QuickViewPlayerVehicleId\" ); " +
            "App.SetUIVariableString( \"ReflexQuickView\", \"p|\" .. id ); App.SendUIEvent( \"GotoVehicleLoadingPage\" ) end; " +
            "PauseMenu.OnQuickViewAIVehicleChanged = function( self ) local v = App.GetUIVariableInt( \"QuickViewAIVehicleId\" ); local r = App.GetUIVariableInt( \"QuickViewAIRiderId\" ); " +
            "local g = 0; local row = App.Lookup( \"AIRider\" ):searchUnique_int( \"AIRiderId\", r ); if( row:isNull( ) == FALSE ) then g = row:cell_int( \"DefaultRiderGearSetId\" ); end " +
            "App.SetUIVariableString( \"ReflexQuickView\", \"a|\" .. v .. \"|\" .. r .. \"|\" .. g ); App.SendUIEvent( \"GotoVehicleLoadingPage\" ) end; end; " +
            "App.DebugPrint( \"REFLEXANSWER:\" .. ok )";
        bool quickViewPatched; float patchRetry;

        // PauseOptions in the world (the game's own pause page) keeps the settings in the game's UI settings (App.SetUISetting): the port
        // gives them its values when a world starts (as OriginalInput does for its settings) and takes the game's back when they change.
        static readonly string[] SyncedSettings = { "EffectsVolume", "MusicVolume", "EngineVolume", "HUDVisibility", "OnlinePlayerName" };
        readonly int[] settingSeen = { -1, -1, -1, -1, -1 }; int[] settingPending;
        int PortSetting(int i)
        {
            switch (i)
            {
                case 0: return Mathf.Clamp(Mathf.RoundToInt((runtime && runtime.gameAudio ? runtime.gameAudio.masterVolume : PlayerPrefs.GetFloat("ReflexAudio.master", 1f)) * 10f), 0, 10) * 10;
                case 1: { var m = FindAnyObjectByType<OriginalMusic>(); return Mathf.Clamp(Mathf.RoundToInt((m ? m.volume : PlayerPrefs.GetFloat("ReflexAudio.music", 0.6f)) * 10f), 0, 10) * 10; }
                case 2: return Mathf.Clamp(PlayerPrefs.GetInt("ReflexUI.EngineVolume", 10), 0, 10) * 10;
                case 3: return runtime && !runtime.OriginalHudVisible ? 1 : 0;
                default: return PlayerPrefs.GetInt("ReflexUI.OnlinePlayerName", 0);
            }
        }
        void ApplySetting(int i, int v)
        {
            switch (i)
            {
                case 0: if (runtime && runtime.gameAudio) runtime.gameAudio.masterVolume = Mathf.Clamp(v, 0, 100) / 100f; PlayerPrefs.SetFloat("ReflexAudio.master", Mathf.Clamp(v, 0, 100) / 100f); break;
                case 1: { var m = FindAnyObjectByType<OriginalMusic>(); if (m) m.volume = Mathf.Clamp(v, 0, 100) / 100f; PlayerPrefs.SetFloat("ReflexAudio.music", Mathf.Clamp(v, 0, 100) / 100f); break; }
                case 2: PlayerPrefs.SetInt("ReflexUI.EngineVolume", Mathf.Clamp((v + 5) / 10, 0, 10)); break;
                case 3: if (runtime) runtime.OriginalHudVisible = v == 0; break;
                default: PlayerPrefs.SetInt("ReflexUI.OnlinePlayerName", v); break;
            }
            PlayerPrefs.Save();
            Debug.Log("[Pages] the game's " + SyncedSettings[i] + " = " + v);
        }
        static int UiSetting(IntPtr w, string name, int value, int mode)
        {
            try { return ReflexInputNative.reflex_world_ui_setting(w, name, value, -1, mode, out int before) == 0 ? (mode == 0 ? before : value) : -1; }
            catch (EntryPointNotFoundException) { return -1; }
        }

        void AskData(OriginalSimulation sim)
        {
            var initial = new int[SyncedSettings.Length]; for (int i = 0; i < initial.Length; i++) initial[i] = PortSetting(i);
            sim.BetweenFrames(w =>
            {
                Command(w, "com_stubs");   // the boot's COM resources' AddRef / Release (pages.cpp): UI material clones
                string v0 = Voice(w);         // the voice hooks, before the live UI's first pages
                string rt0 = Command(w, "ui RulesType");
                lock (gate) { if (!string.IsNullOrEmpty(v0)) voiceLines.Append(v0); rulesTypeAtStart = rt0 ?? ""; rulesTypeFresh = true; }
                string img0 = Command(w, "ui_images");   // the hooks of the images set by code
                if (!string.IsNullOrEmpty(img0)) lock (gate) imageLines.Append(img0);
                var seen = new int[SyncedSettings.Length];
                for (int i = 0; i < seen.Length; i++) { UiSetting(w, SyncedSettings[i], initial[i], 2); seen[i] = UiSetting(w, SyncedSettings[i], 0, 0); }
                bool patched = Lua(w, QuickViewPatchLua) == "1";
                lock (gate) { Array.Copy(seen, settingSeen, seen.Length); quickViewPatched = patched; }
                string rows = Lua(w, PauseRowsLua), stunts = Lua(w, StuntsLua), intro = Lua(w, IntroLua);
                lock (gate) { pendingRows = rows ?? ""; pendingStunts = stunts ?? ""; pendingIntro = intro ?? ""; }
            });
        }

        readonly List<string> pauseRows = new List<string>();
        string challengeTitle, challengeDescription;
        /// PauseMenu:Start's rows for this world (localization keys; empty until the game answered or with an older plugin).
        public IReadOnlyList<string> PauseRows => pauseRows;
        public bool PauseRowsReady => rowsReady;
        /// The ChallengeEventContainer's title / description keys (an activated Free Ride challenge), null otherwise.
        public string ChallengeTitle => challengeTitle;
        public string ChallengeDescription => challengeDescription;
        bool needsFreeRideMap; int freeRideMapIcon = -1;
        /// PauseMenu:Start's needsFreeRideMap (Free Ride, no activated challenge) and the level's HUDMapIconId (EventTypeLocations, EventTypeId 9).
        public bool NeedsFreeRideMap => needsFreeRideMap && freeRideMapIcon != -1;
        public int FreeRideMapIcon => freeRideMapIcon;

        // RefreshFreeRideMap's riders: App.GetNumPlayers, GetPlayerPosition( i ) (x, z), GetPlayerRotation( i ), GetPlayerControllerIndex( i )
        // against the exclusive device (the primary player's when none is set).
        const string PlayerPointsLua =
            "local ex = App.GetExclusiveDeviceIndex( ); if( ex < 0 ) then ex = App.GetPrimaryPlayerDeviceIndex( ); end local o = \"\"; " +
            "for i = 0, App.GetNumPlayers( ) - 1 do local p = App.GetPlayerPosition( i ); local r = App.GetPlayerRotation( i ); local d = App.GetPlayerControllerIndex( i ); " +
            "local l = 0; if( d == ex ) then l = 1; end o = o .. p.x .. \"|\" .. p.z .. \"|\" .. r .. \"|\" .. l .. \"^\"; end " +
            "App.DebugPrint( \"REFLEXANSWER:\" .. o )";
        /// Asks the world for the riders' map points (between its frames); `answer` gets them on the simulation thread (null: none).
        public void AskPlayerPoints(Action<List<(float x, float z, float rot, bool local)>> answer)
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim == null) { answer(null); return; }
            sim.BetweenFrames(w =>
            {
                string s = Lua(w, PlayerPointsLua); List<(float, float, float, bool)> l = null;
                if (s != null)
                {
                    l = new List<(float, float, float, bool)>();
                    foreach (var row in s.Split('^'))
                    {
                        var f = row.Split('|'); if (f.Length < 4) continue;
                        float.TryParse(f[0], NumberStyles.Float, CultureInfo.InvariantCulture, out float x); float.TryParse(f[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float z);
                        float.TryParse(f[2], NumberStyles.Float, CultureInfo.InvariantCulture, out float r);
                        l.Add((x, z, r, f[3] == "1"));
                    }
                }
                answer(l);
            });
        }
        void ApplyRows(string s)
        {
            pauseRows.Clear(); rowsReady = s.Length > 0;
            var parts = s.Split('|');
            foreach (var k in parts[0].Split(',')) if (k.Length > 0) pauseRows.Add(k);
            challengeTitle = parts.Length > 1 && parts[1].Length > 0 ? parts[1] : null; challengeDescription = parts.Length > 2 && parts[2].Length > 0 ? parts[2] : null;
            needsFreeRideMap = parts.Length > 4 && parts[4] == "1";
            freeRideMapIcon = parts.Length > 5 && int.TryParse(parts[5], NumberStyles.Integer, CultureInfo.InvariantCulture, out int icon) ? icon : -1;
            if (rowsReady) Debug.Log("[Pages] pause rows (PauseMenu:Start in the world): " + string.Join(", ", pauseRows) + (challengeTitle != null ? " + " + challengeTitle : ""));
        }

        /// StuntPreview's rows (tier, name, display key), duplicates by display key skipped as PopulateStuntList does.
        public readonly List<(int tier, string name, string key)> Stunts = new List<(int, string, string)>();
        public bool StuntsReady => stuntsReady;
        bool stuntsAnswered;
        /// The world answered (or could not): StuntsReady tells whether it had stunts.
        public bool StuntsAnswered => stuntsAnswered;
        void ApplyStunts(string s)
        {
            Stunts.Clear();
            foreach (var line in s.Split('^'))
            {
                var f = line.Split('|'); if (f.Length < 3 || !int.TryParse(f[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out int t)) continue;
                if (!Stunts.Any(d => d.key == f[2])) Stunts.Add((t, f[1], f[2]));
            }
            stuntsReady = Stunts.Count > 0; stuntsAnswered = true;
            if (stuntsReady) Debug.Log("[Pages] trick library (StuntPreview in the world): " + Stunts.Count + " stunts");
        }

        // ------------------------------------------------------------------------------------------------- the live mirror's state colours and code-set images
        // UIColorState (0x5EAFF0): a component draws the colour of its first state that is on and has one, Focused > Highlighted > Focusable >
        // Enabled, else its colour (+0xd8, the Visible colour, which code may set): the pause list's gold cursor, the focusable rows' grey.
        static readonly Dictionary<OriginalUiTree.Node, uint[]> stateColours = new Dictionary<OriginalUiTree.Node, uint[]>();
        static readonly string[] StateKeys = { "FocusedColor", "HighlightedColor", "FocusableColor", "EnabledColor" };
        static uint Argb(Color c) => (uint)Mathf.RoundToInt(Mathf.Clamp01(c.a) * 255) << 24 | (uint)Mathf.RoundToInt(Mathf.Clamp01(c.r) * 255) << 16 | (uint)Mathf.RoundToInt(Mathf.Clamp01(c.g) * 255) << 8 | (uint)Mathf.RoundToInt(Mathf.Clamp01(c.b) * 255);
        public static uint StateColor(OriginalUiTree.Node t, uint state, uint visible)
        {
            if (t == null || (state & 0xf) == 0) return visible;
            if (!stateColours.TryGetValue(t, out var cs))
            {
                var st = OriginalUi.Obj(t.json, "style");
                cs = new uint[4];
                for (int k = 0; k < 4; k++) cs[k] = OriginalUi.Arr(st, StateKeys[k]) is List<object> l ? Argb(OriginalUi.Col(l, Color.white)) | 0u : 0u;
                bool any = false; for (int k = 0; k < 4; k++) any |= OriginalUi.Arr(st, StateKeys[k]) != null;
                if (!any) cs = null;
                if (stateColours.Count > 20000) stateColours.Clear();
                stateColours[t] = cs;
            }
            if (cs == null) return visible;
            var stl = OriginalUi.Obj(t.json, "style");
            for (int k = 0; k < 4; k++) if ((state & (1u << k)) != 0 && OriginalUi.Arr(stl, StateKeys[k]) != null) return cs[k];
            return visible;
        }

        // The images the page Lua set by code (pages.cpp "ui_images": SetMaterial / SetIconId), by component, resolved as the front end does.
        static readonly Dictionary<uint, (Texture2D tex, Rect uv)> codeImages = new Dictionary<uint, (Texture2D, Rect)>();
        static FeAssets imageAssets;
        public static bool CodeImage(uint obj, out Texture2D tex, out Rect uv)
        {
            if (codeImages.Count > 0 && codeImages.TryGetValue(obj, out var r) && r.tex) { tex = r.tex; uv = r.uv; return true; }
            tex = null; uv = default; return false;
        }
        void ApplyImages(string lines)
        {
            foreach (var line in lines.Split('\n'))
            {
                int bar = line.IndexOf('|'); if (bar <= 0) continue;
                if (!uint.TryParse(line.Substring(0, bar), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint obj)) continue;
                string what = line.Substring(bar + 1);
                try
                {
                    imageAssets ??= new FeAssets(null) { tables = FeTables.Shared };
                    if (what.StartsWith("#", StringComparison.Ordinal)) { int.TryParse(what.Substring(1), NumberStyles.Integer, CultureInfo.InvariantCulture, out int id); codeImages[obj] = id == -1 ? (null, new Rect(0, 0, 1, 1)) : imageAssets.Icon(id); }
                    else codeImages[obj] = imageAssets.Material(what);
                }
                catch (Exception e) { Debug.LogWarning("[Pages] image " + what + ": " + e.Message); }
            }
        }

        // ------------------------------------------------------------------------------------------------- Event Intro Page (not live)
        Canvas introCanvas; OriginalUiTree introTree; bool introBuilt, introFailed, introShown, introDone;
        string introEvent, introLocation; bool introFreeRide;
        public bool IntroShown => introShown;
        public string IntroEventText => introEvent;
        public string IntroLocationText => introLocation;
        void ApplyIntroTexts(string s)
        {
            var f = s.Split('|'); introReady = f.Length >= 3;
            if (!introReady) return;
            introEvent = f[0].Length > 0 ? Localized(f[0]) : "Unknown Event Type Location"; introLocation = Localized(f[1]); introFreeRide = f[2] == "1";
            if (introTree != null) FillIntro();
        }

        static List<Dictionary<string, string>> stringTables;
        /// A string-table key as the game shows it: the install's localiz assets of the in-game packages (InGame / Shared: MXRaven_*,
        /// MXTables_Default_*), then the DLC tables' (FeTables.DlcStrings); the key itself when none has it.
        public static string Localized(string key)
        {
            if (string.IsNullOrEmpty(key)) return "";
            var data = OriginalUiAssets.Data;
            if (stringTables == null)
            {
                stringTables = new List<Dictionary<string, string>>();
                foreach (string pkg in new[] { "InGame", "Shared" })
                    foreach (string name in data.Names(data.Pkg(pkg), "localiz").OrderBy(n => n.StartsWith("MXRaven_Default", StringComparison.Ordinal) ? 0 : 1))
                        try { stringTables.Add(data.Strings(pkg, name)); } catch (Exception) { }
            }
            foreach (var t in stringTables) if (t.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v)) return v;
            try { if (FeTables.Shared.DlcStrings(data.language).TryGetValue(key, out var d)) return d; } catch (Exception) { }
            return key;
        }

        void BuildIntro()
        {
            if (introBuilt || introFailed) return;
            try
            {
                var cgo = new GameObject("Original Event Intro Page", typeof(Canvas), typeof(CanvasScaler));
                cgo.transform.SetParent(transform, false);
                introCanvas = cgo.GetComponent<Canvas>(); introCanvas.renderMode = RenderMode.ScreenSpaceOverlay; introCanvas.sortingOrder = 11;
                var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
                var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
                var root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
                introTree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
                var (pkg, asset) = OriginalLiveUi.LayerPackages.Find("EventIntro.lyr");
                var doc = pkg != null ? OriginalUiAssets.Data.BuildLayerOwn(pkg, asset) : OriginalUiAssets.Data.BuildLayer("InGame", "EventIntro.lyr");
                foreach (var e in OriginalUi.Arr(doc, "elements")) introTree.Build((Dictionary<string, object>)e, root, null);
                introTree.AddAnimations(OriginalUi.Arr(doc, "layerAnimations"));
                introTree.AddComponentAnimations(OriginalUi.Obj(doc, "layerJson"));
                foreach (var r in introTree.roots) OriginalHud.Complete(r);   // icon geometry (the tyres, the banner), as the live mirror
                introCanvas.enabled = false; introBuilt = true;
                FillIntro();
            }
            catch (Exception e) { introFailed = true; Debug.LogWarning("[Pages] Event Intro page not built: " + e.Message); }
        }

        void FillIntro()
        {
            if (introTree == null || !introReady) return;
            OriginalUiTree.SetText(introTree.Find("EventText"), introEvent ?? "");
            var loc = introTree.Find("LocationText");
            OriginalUiTree.SetText(loc, introLocation ?? ""); OriginalUiTree.SetVisible(loc, !introFreeRide);
        }

        void TickIntro()
        {
            // shown from the end of the rules page until the HUD page (the countdown), once per world; never over the live UI (its own page)
            bool want = state.valid && !introDone && state.race == 0 && state.rulesHold == 0 && introReady && !LiveUi && !runtime.paused;
            if (state.valid && state.race >= 1) introDone = true;
            if (want && !introShown)
            {
                BuildIntro();
                if (!introBuilt) return;
                introShown = true; introCanvas.enabled = true;
                if (introTree.HasAnimation("ScenarioHeaderIn")) introTree.Play("ScenarioHeaderIn");   // the page's start (EventIntro.lyr layer animation, 0.4 s)
                Debug.Log("[Pages] Event Intro page: " + introEvent + " / " + introLocation);
            }
            else if (!want && introShown && (introDone || !state.valid || state.race != 0)) HideIntro();
            if (introShown)
            {
                introTree.Tick(Time.unscaledDeltaTime);
                if (AcceptPressed()) SkipScenario();   // the layer's UIKeypressTrigger MapKey_UI_Accept -> SkipToNextScenario
            }
        }

        // the Event Intro page replaces the HUD page (the HUD page starts with the countdown): the port's hand-built HUD stays hidden under it
        void LateUpdate() { if (introShown && runtime && runtime.originalHud && runtime.originalHud.Canvas) runtime.originalHud.Canvas.enabled = false; }

        void HideIntro() { if (introShown) { introShown = false; if (introCanvas) introCanvas.enabled = false; } }

        // ------------------------------------------------------------------------------------------------- Event Outro Page (not live)
        void TickOutro()
        {
            // the live UI's Event Outro page handles its own accept (the keyboard reaches the world)
            if (OutroRunning && !LiveUi && !runtime.paused && AcceptPressed()) SkipScenario();
        }

        static bool AcceptPressed()
        {
            var kb = Keyboard.current; var gp = Gamepad.current;
            return kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || gp != null && gp.buttonSouth.wasPressedThisFrame;
        }

        // ------------------------------------------------------------------------------------------------- localized voice
        readonly StringBuilder voiceLines = new StringBuilder(), imageLines = new StringBuilder();
        string rulesTypeAtStart; bool rulesTypeFresh; float rulesTypeUntil;
        readonly Dictionary<string, uint> voiceWrappers = new Dictionary<string, uint>(StringComparer.Ordinal);
        /// The voice requests played in this session ("play RulesPopup.lyr: SUPERCROSS/SUPERCROSS/RULE_HEAT", tests).
        public static readonly List<string> VoiceLog = new List<string>();
        uint WrapperOf(string owner) { if (!voiceWrappers.TryGetValue(owner, out var w)) voiceWrappers[owner] = w = 0x00fe7101u + 2u * (uint)voiceWrappers.Count; return w; }   // odd: never a game object
        void PlayVoice(string lines)
        {
            var audio = runtime ? runtime.gameAudio : null;
            foreach (var line in lines.Split('\n'))
            {
                if (line.StartsWith("play ", StringComparison.Ordinal))
                {
                    var f = line.Substring(5).Split('|'); if (f.Length < 4) continue;
                    uint w = WrapperOf(f[3]);
                    ReflexUiVoice.Stop(audio, w);   // UILocSound_Play replaces the owner's request
                    bool ok = ReflexUiVoice.Play(audio, f[0], f[1], f[2], w);
                    Note("play " + f[3] + ": " + f[2] + (ok ? "" : " (no such package)"));
                }
                else if (line.StartsWith("stop", StringComparison.Ordinal))
                {
                    string owner = line.Length > 5 ? line.Substring(5) : "";
                    if (owner.Length == 0) { foreach (var w in voiceWrappers.Values) ReflexUiVoice.Stop(audio, w); }
                    else if (voiceWrappers.TryGetValue(owner, out var w)) ReflexUiVoice.Stop(audio, w);
                    Note("stop " + (owner.Length == 0 ? "(all)" : owner));
                }
            }
        }
        void StopAllVoice() { var audio = runtime ? runtime.gameAudio : null; foreach (var w in voiceWrappers.Values) ReflexUiVoice.Stop(audio, w); }
        static void Note(string s) { lock (VoiceLog) { VoiceLog.Add(s); if (VoiceLog.Count > 256) VoiceLog.RemoveRange(0, 64); } Debug.Log("[Pages] voice " + s); }

        void OnDestroy() { StopAllVoice(); }
    }
}

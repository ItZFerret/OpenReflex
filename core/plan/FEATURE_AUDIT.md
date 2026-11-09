# FEATURE_AUDIT: every player-facing feature of MX vs ATV Reflex (PC) against the OPEN REFLEX port

Agent `w-featureaudit`, branch `w-featureaudit` (from main 85e1684), 2026-10-27. Read-only investigation: no game code was changed and no
player was run (no showcase, no headless tool). Every status below comes from reading the code at 85e1684, the QUEUE.md records and the test
records they cite. Where a status rests on code reading alone and not on a recorded run it says so ("(code)").

## 0. Method, sources, legend

**Original side** (what the PC game offers): the three page graphs exported from the install's UI packages
(`_re/formats/out/FrontEndPageGraph.xml` 50 pages, `ui/InGamePageGraph.pag.xml` 39, `ui/TransitionPageGraph.pag.xml` 11; the FrontEndIntro graph via
`_re/notes/port-audit.md` sec. 4), the 104 shared + 80 ENG Lua scripts and 92 layers under `_re/formats/out/ui`, the string tables
(`ui/loc/ENG/*.json`, `_re/out/strings.tsv`), `MXTables_Default.xml` (events, products, unlocks, cheats), `KeyBind_*.xml`, `ControllerPresets.xml`,
the earlier audits `_re/notes/port-audit.md`, `modes.md`, `replay.md`, `in-race-pages.md`, and the decompile names (`_re/names`).
**Port side**: `Unity/Reflex/Assets/Scripts/OpenFrontEnd` (the OPEN shell, `Own*` race pages, `Oc*` career kit), `Options/`, `Hud/`, `OpenReflex/`,
`OpenReflexBridge/`, `OriginalWorld/` (the race runtime, `OriginalPauseMenu` fallback), `Content/`, and the core (`core/api`, `core/include`,
`core/plan/STOP_AUDIT.md`: 0 NEEDS-PORT, 51 OUT-OF-SCOPE).

**Which front end counts.** The product is the OPEN player (`REFLEX_SIM=open` / the packaged marker): the OPEN shell, the C core, no emulator and
no Lua. The older reconstruction of the original front end (`Scripts/FrontEnd/*`, the original pages driven through the emulator's profile session)
still exists in the tree and is used only with the legacy simulation; a page implemented only there is reported **MISSING** here (with a note
saying so), because the open player cannot reach it (`OpenShell.ShowGarage` / `OpenOriginalFor` route Career and Garage to the OPEN pages when the
core is on).

**Status words**
- **DONE**: implemented in the open product and reachable (how, in the notes). Most rows cite a recorded test run; "(code)" marks the exceptions.
- **PARTIAL**: reachable, but something the original does is absent (named in the notes).
- **MISSING**: no OPEN implementation.
- **DEFERRED**: the lead decided it is out of the current scope (cheats; party modes; championships; startup videos / title page; online).
- **IN PROGRESS**: another agent is on it (`REFLEX-w-tutorials`: the MotoSkills tutorials).
- **N/A**: the PC game itself cannot reach it (console, demo, dead code, no data rows) or the thing does not exist in the original.
- **EXTRA**: the port has it and the original does not.

Difficulty scale in sec. 12: **S** under a day, **M** a few days, **L** a week or more, **XL** several weeks. "Core" says whether a port of
original functions into `core/` is needed or the work is Unity-side only.

## 1. Lead decisions applied (memory + QUEUE)

| Decision | Applied as |
|---|---|
| Original cheat codes are not wanted (the user designs their own) | cheat entry (Options) and the 14 codes: DEFERRED |
| Party modes (Tag, Snake, Capture, EggHunt, Infection, Graffiti, PokerRace) wait for the Open-Unleashed-style online framework; Section Race stays in scope | DEFERRED; Section Race DONE |
| Championship weekends wait for the "restored content" pass (with the other cut / unreachable content) | DEFERRED |
| Startup intro videos and the title "start" page are intentionally off | DEFERRED |
| Credits are the project's own (itswillyferret, Steeler, boneboy, THQ / Rainbow with the not-affiliated notice) | the original Credits page: DEFERRED (replaced) |
| Free Ride at FR_Alpine / FR_Gibson / FR_Cheyenne runs with InCareerMode on (activation areas + discovery in the arcade Free Ride) | kept; DONE |
| Debug tools (debug camera, dev console, Dev Menu) are optional | N/A |
| Online / Quazal stays a boundary | DEFERRED |

## 2. Shell, startup and navigation

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| A01 | Startup videos (Legal screen, publisher, studio, DH logo, attract video) | FrontEndIntroPageGraph: Legal Screen Page, Publisher Video Page (Nordic_Logo_wSound.bik), Studio Video Page (Rainbow_Logo), Studio Video Page2 (DHLogo), Intro Video Page (Attract.bik) | DEFERRED | off by default (`OpenReflex.cs` OriginalIntro; `REFLEX_INTRO=1` / PlayerPrefs `OpenReflex.Intro` turns it on for development). Memory: scope-no-intro |
| A02 | Title / "press start" page, Attract page loop | StartGamePage, Title Page (Title.lua), Attract Page | DEFERRED | the shell boots straight to the hub |
| A03 | Main menu | MainMenu.lua: ARCADE, CAREER, MULTIPLAYER, MOTOCARD list; legend OPTIONS, TUTORIALS, QUIT, DEV | DONE | hub = RACE / FREE RIDE / CAREER / GARAGE / OPTIONS, chips GAME LIBRARY / CREDITS / QUIT, CONTINUE card, MOTOCARD card (`OpenShell.ShowMain`). ARCADE is split into RACE and FREE RIDE; MOTOCARD opens GARAGE; MULTIPLAYER see J; TUTORIALS see I |
| A04 | Quit confirmation | QuitConfirmationPopup | DONE | hub QUIT asks first: `OpenShell.ConfirmQuit` (`OpenShell.Popups.cs`), the original's QuitConfirmationPopup words (STR_QUIT, STR_QUITGAMEPOPUP_MESSAGE, YES focused as the Lua's SetFocused, NO, Back = NO) (w-p2front)  |
| A05 | Loading screens between menu and race, with progress | TransitionPageGraph: InGame / Main Menu / FrontEnd / Career / Career Race and Ride / Test Drive Loading Pages (LoadingScreen.lyr, Background01.lyr) | DONE | `OpenLoading` (location art, world build progress), `OwnAaaLoading`, `CareerCurtainPresenter` |
| A06 | Page wipe, header bar, button legend band | PageWipe.lua, HeaderLayer.lua, ButtonLegend.lua | DONE | `OcUi.Legend` / `OuUi.Hints`, `OuTheme.Header`, PageWipe in `OwnAaaLoading` (w-polish) |
| A07 | UI sound effects and tiered front-end music | UIRegistry.xml `UISound`; App.SetUIMusicTier (hub 2, event select 3, location select 5) | DONE | `FeSounds` / `FeUiMusic`, `OpenShell.MusicTier` |
| A08 | Navigation by keyboard, mouse and pad in every menu | KeyBind_Core.xml MapKey_UI_*; MouseActivation_* handlers | DONE | `OuNav` + `OuMenuInput` (hub, browser, setup, garage, career, options, results, Own pause). The gap is the fallback pause page: F01 |
| A09 | Profile sign-in at the title, load game, profile required / reminder popups | Title Page -> Load Game Page (TitleLoadGame.lua), Profile Login Status Updated Page, Profile Required/Reminder popups | N/A | on PC this is the Steam user's single game save; the console storage-device flows (ProfileSelection, StorageNotification / Question / Removal popups) are PS3 / Xbox. The port opens its one local profile silently (`ReflexProfile.Open` -> `CoreGarageProfile`); the corrupt-save question is answered by the daily backups (`ReflexSave.BackUp`) |
| A10 | Several motocards / profile slots | none on PC: TitleLoadGame loads the one save; MotocardNameEntry's "unique across all MotoCards" is the console store | N/A | the port has one career profile and an UNLOCK EVERYTHING clone (EXTRA). A slot picker would be a port addition |
| A11 | First-run Motocard setup wizard | Autosave Notice Page, Motocard Setup Name Page, Screen Adjustment Page, Welcome, Vehicle, Gear, Skintone, Jersey Name, Jersey Number, Completion (MotocardSetup*.lua) | DONE | the MOTOCARD welcome (`OpenShell.Welcome.cs`: STR_MOTOCARD_WELCOME_TITLE / _MESSAGE, GRAB YOUR RIDE!, once on a profile that has not played; PlayerEvent "MotocardWelcome") and the SCREEN ADJUSTMENT page (`ReflexOptionsMenu.Screen.cs`, also a button on the welcome); the name, jersey, gear, skin tone and vehicle steps are the GARAGE's tabs (the profile starts set up). No autosave notice (PC saves on every change). The page plays the original's narration (UI_Game_Intro `UI_GAME_INTRO/VO/INTRO`, stopped when it goes: `OpenShell.Narrate`) (w-p2front, w-partials)  |
| A12 | Cursor overlay (software mouse cursor) | CursorOverlay.lua | DONE | OS cursor; UI Toolkit pointer events |
| A13 | Demo build pages (DemoSplash, DemoVideo, DemoMainMenu, Demo Controller Layout) | DEFINE_Demo branches | N/A | |
| A14 | Dev menu, load-level page, debug join | Dev Menu Page, LoadLevelPage, Join Network Game (hidden in RELEASE) | N/A | memory: debug tools optional |
| A15 | Credits page | Options > Credits -> Credits Page (Credits.lyr) | DEFERRED | replaced by the project's own CREDITS (`OpenShell.Credits.cs`, `OpenCredits`, Credits.json) per the lead |

## 3. Arcade: RACE and FREE RIDE (Event Select, Vehicle Select, Location Select)

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| B01 | Event type chooser with posters and rules text | Event Select Page (EventSelect.lua: Free Ride, National, Supercross, Terracross, Freestyle, Powercross, Waypoint; STR_*_DESCRIPTION / *_VEHICLES) | DONE | track browser filters by event type in the original's order with counts + the type's text on the setup page (`OpenShell.Browser.cs`, `OpenContent.TypeOrder`) |
| B02 | Vehicle Select (Player 1): vehicle type MX Lite / MX / ATV (+ machines), My Rides, pro riders, 3D preview | Vehicle Select Page (VehicleSelect.lua, VehicleSelect_Cameras.lua) | DONE | the setup page's RIDER tab (VEHICLE / CLASS / RIDER spinners) now shows the choice in 3D: the location picture gives way to a window on the 3D menu world (the hero spot), the vehicle and rider the choice comes to (YOUR RIDER on the garage's vehicle of the type with the equipped gear; a pro on his own vehicle), built on a worker thread as it changes so navigation is never blocked (`OpenShell.Setup.cs`, showcase page `setup-vehicle`: MX Lite own, MX / ATV with James Stewart). Left: the right panel's glass is lighter than the left cards over the preview |
| B03 | Location Select: posters, preview / map toggle, new and DLC marks | Locations Page (LocationSelect.lua) | DONE | browser cards with location pictures, track map, description, DLC pill, grid / list, search, favourites (EXTRA) |
| B04 | Event duration (laps / minutes) | LocationSelect EventTypeDurations spinner | DONE | `OpenSetup.Durations`, `Launch(... laps)` |
| B05 | AI skill NONE / ROOKIE 80 / AMATEUR 95 / PRO 105 / ALL TIME 120 | LocationSelect:BuildAISkillList | DONE | `OpenContent.AiName`, AI OPPONENTS spinner |
| B06 | Vehicle class of the event (Single, Rival "MX vs ATV", Multi "Moto / Machines / Unlimited") | LocationSelect:BuildVehicleClassList; VehicleClass.Format | DONE | CLASS spinner; the AI field's own vehicles by the game's generator (w-vfactory, `ReflexEvents.RaceableClassFormats`) |
| B07 | Arcade progression locks: locations and AI levels start locked and are unlocked by career rewards (or cheats WHERETO / ALLAI) | LocationSelect isLocked = MXTableHelper:IsLocked (no PlayerProductsUnlocked row = locked; 31 default rows); SeriesRewardsUnlock (231 rows); STR_CHEATCODE_WHERETO / ALLAI | DECIDED (lead, 2026-10-28): arcade stays fully open | the browser lists every event unlocked, and stays so by decision (nothing to build); the original locks locations and AI levels until the career unlocks them (PlayerProductsUnlocked). Counted with the lead's decisions (DEFERRED column) |
| B08 | RULES button: the event type's rules popup | Event/Location Select legend STR_RULES -> RulesPopup | DONE | the RULES chip on the setup page of the event types with rules (Freestyle, Section Race: MXUIManager 005a4bd0) and the same rules page before START EVENT until CONTINUE, DO NOT SHOW AGAIN (PlayerEvent "Rule_<type>"); the original's LocationSelect "RULES" toggle is the rules lists (= the setup page), not this popup (w-p2front) w-hudrest: the page plays the original's narration (RulesPopup.lua: `ReflexUiVoice.RulesVoice(RulesType)` = "AudioLocalizedActivity", "Audio_Loc_<ACTIVITY>", "<ACTIVITY>/<ACTIVITY>/RULE") through `OpenShell.Narrate` while it is up (the RULES chip, the rules before START EVENT, the challenges' Free Ride rules), stopped when it closes; EFFECTS VOLUME drives it (`ReflexClientLive.EffectsVolume` now also sets the open shell's sounds). |
| B09 | Event rows that run | EventTypeLocations 64 rows (+6 DLC): National 8+2, Supercross 8+2, Terracross 6, Powercross 4, Freestyle 6+2, Waypoint 6, Section Race 5, Free Ride 3 | DONE | `ReflexEvents.All` (RunnableActivities), every row's whole life run headless by the stop-audit matrix M1 (420 runs); in Unity the arcade probe covers National (NAT_Stillwater) and Free Ride FR_Alpine; the other types' Unity pause / results are F01 / F13 |
| B10 | Free Ride (open world, discovery, activation areas) | FreeRide activity; AAA areas | DONE | `FREE RIDE` hub entry, all three maps, arrival scenes (w-frscene), AAA areas (J: C13) |
| B11 | Waypoint Race (compass, gate arrows) | Waypoint Single Player Page | DONE | HUD compass + strip (`ReflexHudView.Modes`) |
| B12 | Recent events, CONTINUE / RIDE NOW, favourites, sort, search, grid / list | not in the original | EXTRA | `OpenHistory` |
| B13 | Split screen: Player 2 Vehicle Select, Split Screen Controller Select, split HUDs and divider | pages exist; MainMenu.lua line ~300 has the Multiplayer Select event commented out | N/A | unreachable on PC; the core runs one local human (the 3-viewport branch of 00757660 is noted as unported for a split in the w-replay QUEUE section). A restored-content / port addition: XL, core |

## 4. Career (Race and Ride)

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| C01 | Race and Ride hub: series tiers, events with medal progress, locked series | Race and Ride Page + Race and Ride Events (RaceAndRide.lua, RaceAndRideEvents.lua) | DONE | `OcRaceAndRide` (carousel of tiers, events list, medal slots, COMPLETE / PODIUM tags); every admitted series of the catalog selectable |
| C02 | Career formats: Single, Moto (2 motos), Heat / LCQ / practice qualifier, Eliminator, Faceoff, Outrun, Overtake, Section Race, Freestyle, Waypoint | Series.Format, SeriesEventRules; CareerPracticePopup | DONE | w-career: series admitted by the original's own rules, versioned presenter capabilities; moto / heat flows have Unity pages (`OwnCareerPages`, practice choice popups). Recorded runs: Moto14, `--series N` probe table in QUEUE (w-career) |
| C03 | Start Race popup, Rules popup per format | StartRacePopup, RulesPopup (RulesType) | DONE | RulesPopup done (`career-rules-accept`, DON'T SHOW AGAIN). w-startrace: StartRacePopup (`OpenShell.ShowStartRacePopup`, called by `ChooseCareerEvent` = RaceAndRideEvents:OnAcceptSeries's order: locked -> invalid vehicle -> Moto / Heat PRACTICE page -> StartRacePopup for every other format): STR_START_EVENT_TITLE "START EVENT" / STR_START_EVENT_MESSAGE "Are you sure you want to start the event?" / YES (focused, as the Lua's SetFocused) / NO, the popup page's SELECT / BACK legend; A on YES launches (the rules popup and the gate selection then follow inside the event as before), NO and B close it (TravelBack) and the focus returns to the event row; the launch is latched against a second A. The original popup shows only the question; the event card above the buttons (name, place, format, laps, vehicle class, the player's vehicle) is the port's addition from the events page's data. Pad journey in the showcase `startrace-popup` (A opens, right / left, B back on the row, A, NO, A, A on YES launches, locked and Moto rows unchanged). Also fixed: `OuP2Front.uss.meta` was malformed (the whole sheet, i.e. every popup / credits style, was ignored in a fresh build) w-hudrest: the career's rules page (`OwnCareerPages` RulesShow) plays the RulesVoice of its RulesType on the race's event player and stops it with ACCEPT / DON'T SHOW AGAIN / the page ending. |
| C04 | Gate selection with the camera on the gates | Gate Selection Page (GateSelection.lua, App.ShowGates / SetCameraToGate) | DONE | w-p2scene: the career's Moto / Heat gate page is now the 3D view: the world's camera on the focused start gate (look-at = the GateStart_NN node, eye = 25 behind and 7 up, a 0.5 s quintic blend per move, SetCameraToGate's own formula in the gate model's space through the gate entity's matrix), the level's numbered gate cards (UI_StartCard_NN) over every gate, the X card on the gates the riders ahead took, the yellow arrow on the focused one; the gates are a strip above the legend (left / right move the focus and the camera, A selects). The first round's page (before the activity exists) loads the level early; riders of a previous round are hidden. Core: read-only `rx_session_start_gates` (rx_scene_pages_api.h). Run in the real player: Heat series 41 (round 1; LCQ in an earlier run). The Moto page is the same code (the Moto14 probe hangs at its garage step on the unmodified build too). Left: the stick's small orbit of the parts-view camera is not drawn; the arcade has no gate selection in the original |
| C05 | Career results: standings, points, moto results table | Post Event Moto Results Page, PostEventResults, PostEventOverlay | DONE | `OwnCareerPanels`, `OwnRaceResults` career variant |
| C06 | Rewards page: credits, bonus, unlocks | Post Event Reward Page (PostEventRewards.lua) | DONE | `OwnCareerPanels.ShowRewards` (credits / bonus); unlock rewards applied by the core (career_rewards.c) |
| C07 | Vehicle reward presentation | Vehicle Reward Page (RewardVehiclePresentation.lua) | N/A | no PC series rewards a vehicle (`SeriesRewardsPurchase` names no VehicleModel; `career_public_session.inc` comment) |
| C08 | Career Welcome page (first entry; AI difficulty choice with narration) | Career Welcome Page (CareerWelcome.lua; STR_CAREERWELCOME_*) | DONE | `OpenShell.ShowCareerWelcome` (`OpenShell.CareerPages.cs`): first CAREER entry (PlayerEvent "CareerWelcome"), STR_CAREERWELCOME_*, the difficulty list 80 / 95 / 105 / 120 (STR_AI_LEVEL1..4), accept stores CareerDifficulty. The page plays the original's narration (UIAudio UI_MOTO_INTRO `UI_MOTO_INTRO_ENG/VO/INTRO` through the shell's FeSounds, stopped with the page) (w-p2front, w-partials)  |
| C09 | Career Completion page (video at 100 %) | Career Completion Page (CareerCompletion.lua; UIVideoComponent, App.CalculateCareerCompletion) | DONE | `OpenShell.ShowCareerCompletion`: at 100 % (`OpenMotocardData.CareerCompletion`) once (PlayerEvent "CareerCompleted"): Videos\CareerCompletion.bik then Videos\Credits.bik through `OriginalVideo` / IBinkSource (ffmpeg in the open player), UI music muted; Back / Start skip (a port courtesy) (w-p2front)  |
| C10 | Vehicle requirement: Invalid Race Requirements popup, quick vehicle swap, link to garage | InvalidRaceRequirements popup, VehiclesQuickViewPopup from Race and Ride Events / Challenges | DONE | the InvalidRaceRequirements popup (STR_INVALID_VEHICLEREQ_*) when a series for another vehicle class is chosen, an INVALID VEHICLE tag on those rows, and the VEHICLE LIST quick swap (Y / F or the legend item: `ShowCareerVehicleList`, VehicleClassType types, columns STR_VEHICLE_TYPE / _NAME, performance bars, accept equips and the hub is read again) (w-p2front)  |
| C11 | Locked popup (what unlocks it) | LockedPopup | DONE | LockedPopup (STR_LOCKEDPOPUP_*) for a locked event row or a fully locked poster, as RaceAndRide(Events):OnAcceptSeries (w-p2front)  |
| C12 | Player information panel (equipped vehicle, balance) | CareerPlayerInformation.lua | DONE | the hub's top right shows CREDITS (the garage's balance, ALL UNLOCKED on the unlocked profile); the original's Race and Ride page itself shows only the vehicle (w-p2front)  |
| C13 | Free Ride challenges page (Race and Ride > Free Ride series: per-level challenge list with medals, start) | Race and Ride Challenges Page (RaceAndRideChallenges.lua, product 2904) | DONE | `OpenShell.ShowChallenges` (`OpenShell.Challenges.cs`, `OpenChallengeData.cs`): FREE RIDE > CHALLENGES; the three Free Ride series as cards (medal squares from PlayerChallenges), per location the Free Ride photo and each challenge with its medal, GOLD / SILVER / BRONZE thresholds (the level's MXChallengesSpecEntity via `ReflexChallengeSpecs`), the stored best, and START (the challenge's own world; Hit Discovery and the RIDE button start the series' Free Ride); locked series answer with the LockedPopup; back from the race comes back to the page (w-p2front)  |
| C14 | Career save / checkpoint feedback | Save Popup; autosave | DONE | `OpenShell.CheckpointNotice`, durable checkpoints (w-career) |
| C15 | Championship weekends (Intro, Practice, Heat, LCQ, Main Event) | MXActivity*Champ* classes; tables empty on PC | DEFERRED | restored-content pass (code ported and verified by w-champ / w-freestyle) |
| C16 | Career vehicle gating / series unlocks | IsLocked chain, SeriesRewardsUnlock | DONE | `CareerEventChoice.Locked / Eligible` from the core's rules |

## 5. Garage and Motocard

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| D01 | Garage hub with the 3D compound, rider beside the bike, push-bike walk-in | Garage Page (GarageHUB.lua, GarageMenu.lyr), UI_Compound | DONE | `OpenShell.Garage*` + `OpenMenuWorld` (w-menu3d) |
| D02 | Customize Rider: gear (helmet, goggles, jersey, pants, boots, gloves, neck brace, chest protector), skin tone, purchase with credits | Garage Customize Rider Page (GarageRiderCustomize.lua) | DONE | GEAR tab, strip of owned / for sale / locked, PURCHASE and NOT ENOUGH CREDITS dialogs (`GaragePurchase`) |
| D03 | Jersey name and number | Jersey Name / Number Entry popups | DONE | RIDER tab (`garage-jersey-name`, NUMBER) |
| D04 | Motocard name | Motocard Setup Name Page, InvalidMotocardNamePopup | DONE | RIDER tab NAME row (`ReflexProfile.Persona`) |
| D05 | My Rides: vehicles by class, buy / equip, DLC and NEW marks | Garage My Rides Page (GarageMyRides.lua) | DONE | VEHICLE tab, class filter, EQUIPPED / LOCKED / price pills |
| D06 | Customize Vehicle: parts, graphic kits, colours | Garage Customize Vehicle Page (GarageVehicleCustomize.lua) | DONE | PARTS tab (one row per customizable part) |
| D07 | Vehicle tuning (engine, suspension, tires, braking, steering) | VehicleTuningPopup (GarageVehicleTuning.lua) | DONE | TUNING tab; applied to the core (`rx_session_tuning_set`) |
| D08 | Vehicle performance display | VehiclePerformanceDisplayComponent.lua | DONE | stat bars on the stage |
| D09 | Pro rider / pro vehicle selection | Vehicle Select pros; Garage | DONE | PRO RIDERS cards (`garage-pro-*`) |
| D10 | Test drive | GarageHUB TEST DRIVE -> Test Drive Loading / Exit Pages | DONE | TEST RIDE (w-testride, w-ridefx): the compound as a free ride, pause = KEEP RIDING / RETURN TO GARAGE |
| D11 | Motocard page: rider stats, career completion %, credits, recent highlights | Motocard Page (Motocard.lua: PopulateLocalInformation, CalculateCareerCompletion) | DONE | MOTOCARD pages (`OpenShell.Motocard.cs`, w-motocard): hub MOTOCARD card / garage RIDER header, live portrait, name, number, MOTOCAREER PROGRESS (00743870 reimplemented in C# `OpenMotocardData.CareerCompletion`, not a core port; the profile is a managed snapshot in the product), credits, recent achievements. The original has NO events-played / podium / distance rows locally (GetMotocardStats / PlayerMinedStat = the online upload); RECORD TOTALS are a labelled port extra. Showcase `motocard-*`, `--walk` 0 failures |
| D12 | Highlights (the 48 achievements, progress, unlock order) | Motocard Highlights Page (MotocardHighlights.lua; Highlight, PlayerHighlight; Steam achievements) | DONE | ACHIEVEMENTS tab: 48 rows, icons (locked ones grey), titles, descriptions, progress, HighlightSort order, detail panel (`OpenMotocardData.Highlights`); no unlock toast yet |
| D13 | Records: offline leaderboards by event / location / vehicle (best lap, best total, highest score) | Motocard Leaderboard Page (MotocardRecords.lua; STR_LEADERBOARDS_OFFLINE) | DONE | RECORDS tab: the original's activity list and sort, locations with posters, PlayerRecords rows by vehicle type, column by event flags (005a3820), FormatTime (00790100). Offline only; online half: J |
| D14 | Stunts page and trick preview | Motocard Stunt Page (MotocardStunts.lua), StuntPreview.lua | DONE | TRICK LIBRARY tab (w-motocard) by tier with gesture pictures from `CoreStuntCatalog`, plus the pause's TRICK LIBRARY. w-partials: the detail panel has a live preview (`OpenStuntPreview`): the player's rider in his gear on his equipped MX Lite / MX / ATV (MotocardStunts' rule) in a second picture-mode `FeGarage` (low-resolution models as the original's `SetHREnable( FALSE )`), A plays the focused trick (StuntPreview:OnAcceptStuntList = MXGarage:PreviewStunt): the StuntTable name's perfdat `<MX|ATV>Rider_<name>` (`ReflexStuntAnim`: In / Hold (looped) / Out clips of the Animations database, the rider's through `ReflexRiderAnim`, the vehicle's `<MX|ATV>Vehicle_...` clip moving the vehicle by its OBJ_Frame track). The menu world's own hero is behind the Motocard's opaque page, so the preview has its own garage. Not animated: the five physics tricks (Wheelie, Stoppie, Backflip, ThreeSixty, Whip: no perfdat in the data; the page says so), the vehicle's fork / steering-stem track, and 10 of the 38 animated tricks have no vehicle clip in the install (Cordova, Deadbody, Heel Clicker, Lazyboy, No Hander, Shaolin, Superman, Switchblade, Side Saddle, No Footer: the rider alone moves). The original keeps no per-trick counts or scores, so none are shown w-hudrest: on an ATV the performance (`ATVRider_<stunt>`) carries additive blocks (`ATVRider_AD_StuntStart` / `_PelvisDown` / `_PelvisBackward`, timeScale 14.8-16 so each spans the whole stunt) that the preview played as extra steps of the timeline (a crumpled pose between In and Hold); they are now laid over the pose the way the PerformanceAnimator does (0083e930: p' = slerp(p, p x t, weight), translation += t x weight, the clip run once over its span), and the vehicle clip is the first plain step's; the physics tricks (WHEELIE ...) show "a physics trick" on the ATV as on the MX Lite (no `ATVRider_Wheelie` perfdat). |
| D15 | Steam achievements | 48 Highlights -> Steam | N/A | no Steam in the port; see D12 for a local list |
| D16 | Cheat code entry and unlock codes | Options > Cheat Entry Popup; CheatCode / CheatCodeProducts | DEFERRED | the port has UNLOCK EVERYTHING (EXTRA) instead |

## 6. Options, controls, languages

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| E01 | Options page settings: driving camera 1-4, brightness, gamma CRT / LCD, HUD visibility, online player name, measurement system, effects / music / engine volume, career difficulty, vibration, mouse sensitivity | OptionsFrontEnd.lua PopulateSettings | DONE | `Options/ReflexOptionsMenu*.cs`: all of them (CONTROLS, AUDIO, VIDEO, GAMEPLAY, CAMERA & HUD, INTERFACE, SYSTEM), also in a race from the pause |
| E02 | Controller / keyboard remapping | Controller Remap Page (ControllerRemapping.lua) | DONE | CONTROLS > CONTROLLER REMAPPING table, written to the core profile (`CoreRemapping`, `rx_session_remap_set`); refuses Q / E / 3 / Alt like the game |
| E03 | Controller preset page (6 presets, handedness, motion) | Options Controller Page (OptionsControllerPreset.lua) | N/A | nothing sends `GotoOptionsControllerPage` in the PC Lua. The port still has STICK LAYOUT (ControllerHandedness) |
| E04 | Graphics settings (MXSettings.exe / graphics.cfg: resolution, AA, textures, shadows, motion blur, fullscreen, vsync) | graphics.cfg; MXSettings*.dll | DONE | VIDEO tab (`ReflexLaunchSettings`) |
| E05 | Language | UIRegistry.xml ForceLanguage; MXSettings{DEU,ESN,FRA,ITA}.dll | DONE | INTERFACE > LANGUAGE (limited to the languages the install carries) |
| E06 | Screen adjustment page (safe frame / gamma ramp) | Screen Adjustment Page | DONE | OPTIONS > VIDEO > SCREEN ADJUSTMENT: the original's Screen Adjustment Page (title / message, GAMMA CRT / LCD list, CONTINUE) with grey steps to adjust against, plus BRIGHTNESS (w-p2front)  |
| E07 | Storage device selection, save question popups | OptionsFrontEnd OnSelectStorageDevice | N/A | console |
| E08 | Save backup / restore, open save folder | not in the original | EXTRA | SYSTEM > SAVE DATA |
| E09 | Mouse lean, rumble strength, UI scale, reduce motion, controller glyphs | not in the original | EXTRA | `ReflexSettings` |

## 7. In-race: pause, intro, HUD, cameras, replay, results

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| F01 | Pause menu: RESUME, RESTART, TRICK LIBRARY, OPTIONS, QUIT (+ confirm popups) | Paused Menu Page (PauseMenu.lua) | DONE | `OwnRacePause` for every offline mode (w-pausemodes): National, Supercross, Terracross, Powercross, Waypoint, Freestyle, Section Race, Free Ride and its challenges, the career's series events (no RESTART there); the rows are PauseMenu.lua's for the mode (`OwnModes.Pause`: no RESTART in the Free Ride, VEHICLE TUNING / VEHICLE LIST / the map there, an activated challenge keeps RESTART and QUIT goes back to the Free Ride) with the mode's own stat tiles. Start / Esc opens it (the start countdown and the event in progress), Start / F3 closes it, d-pad / stick / A / B drive it (OuNav). Run in the real player with a virtual pad: Free Ride, Freestyle, Waypoint, Section Race, Terracross, the Race & Ride Free Ride. `OriginalPauseMenu` stays only for the worlds the owned page does not cover (the MotoSkills tutorials); it, `OriginalConfirmPopup` and `OriginalPostRace` now read the pad too (`OriginalPadInput`). Not run in a real career race (code + showcase) |
| F02 | Free Ride pause rows: VEHICLE TUNING, VEHICLE LIST (quick swap of vehicle and rider), the Free Ride map with riders, no RESTART | PauseMenu.lua needsVehicleTuning / needsQuickView; VehiclesQuickViewPopup | DONE | done in `OwnRacePause` (w-pausemodes): VEHICLE TUNING (the garage's rows, saved and put on the running physics), VEHICLE LIST (the player's vehicles with their bars; a choice equips it and loads the same Free Ride with it, a new session as the legacy path did), the FREE RIDE MAP (the location's HUDMapIconId with the rider's arrow), no RESTART. w-partials: PRO RIDERS list (Y / F toggles MY RIDES / PRO RIDERS as the popup's Toggle button): every PlayerSelectable AIRider x his AIRiderVehicle rows of the Free Ride's vehicle types (`OpenSetup.VehicleTypes`), columns STR_PRO_RIDER / STR_VEHICLE_TYPE, a choice = the AI vehicle + the rider in his DefaultRiderGearSetId (AI owners, this session only; VehiclesQuickViewPopup:OnQuickListAccept + PauseMenu:OnQuickViewAIVehicleChanged). Note: the original builds the types from the UI variable VehicleClass, which LocationSelect leaves at -1 in the arcade Free Ride (no rules), so its pro list is empty there; the port lists the types the Free Ride can ride. **N/A (offline):** the original's Free Ride map also plots the other (network) riders (RefreshFreeRideMap loops the players); offline the Free Ride has only the local rider, whose arrow is drawn. |
| F03 | Restart / quit event | Restart / Quit Event Popups | DONE | `runtime.RestartEvent`, `QuitToFrontEnd`; challenges restart in place (`rx_session_challenge_restart`), career events too (`rx_session_restart_activity`, w-careerrestart); an arcade event starts a new session |
| F04 | Event intro: start-line overview, cast, hero shots, crew chiefs, intro skip | EventIntro scenarios (RaceIntroScenario, FMXIntroScenario, WaypointIntroScenario) | DONE | cast and cameras drawn from core state (w-cast, `OriginalPodium`); the skip: Enter / A (the page's accept = App.SkipToNextScenario, new core call `rx_session_skip_scenario` sets [0xa7f748]) and the pad's X (the game's own ScenarioNextButton, input 0xe, in IntroScenario_Update); run in the real player (arcade National: skip requested 4.1 s into the intro, the countdown state follows) |
| F05 | Event Intro title card (event name over location) and Event Outro skip | Event Intro Page (EventIntro.lua), Event Outro Page | DONE | w-p2scene `OwnEventIntro`: while the intro scenario plays (Pre, rules hold clear) the event's title (EventTypeLocations -> StoreProducts title) on the dotted patch over its location (EventLocation; hidden in the Free Ride) on the orange bar, in 0.4 s, the HUD hidden under it (the original's HUD page starts with the countdown); a SKIP INTRO legend fades in after 1.2 s (a port addition: the original page has no hint text). Arcade National and the career Heat run in the real player. The Event Outro: EventOutro.lyr is an empty layer whose accept sets the same word, but only IntroScenario_Update reads it, so the PC original has nothing to skip in the post scenarios: no hint built there |
| F06 | Start gate, countdown 3-2-1-GO, Powercross board | BeginningCountdown components | DONE | HUD centre band (`ReflexHudView`), gate drop |
| F07 | Rules hold popup before arcade modes | RulesPopup (activity +0xff068) | DONE | the arcade rules popup is the front end's (B08); career: DONE; the challenges Free Ride shows the "Freeride" rules from the CHALLENGES page (w-p2front) w-hudrest: the rules narration plays here too (see B08). |
| F08 | HUD per mode: Race (position, lap, lap times, gaps, speed / gear), Eliminator, Overtake, Outrun, Section Race, Freestyle (judges, final card), Waypoint, Free Ride, Hill Climb, Trailblazer, Target Practice | the SinglePlayer*.lyr layers | DONE | `Hud/ReflexHudView*.cs` modern HUD, HUD STYLE = MODERN / ORIGINAL (`OriginalHud.RaceHud.cs`), standings board, gauge; trick names and WarningTime flash (w-polish2) |
| F09 | Hit Discovery counter / Free Ride discovery HUD | MXUIHitDiscoveredComponent (Free Ride page) | DONE | w-pausemodes: `rx_session_race_hud` carries `discovery_events` / `discovery_found` / `discovery_total` (the discovery tracker's counts, discovery mode = the Race & Ride challenges Free Ride); the MODERN HUD draws a DISCOVERIES n / m panel under the compass and flashes it on a find (the original shows "n/m" for 3 s); the pause tile shows it too. w-partials: HUD STYLE ORIGINAL draws it too (`OriginalDiscoveryCounter`: the Free Ride layer's own "Hit Discoverd Component", icon and text, HitDiscoveryIn / Out, "n/m" for 3 s on a find, 006045b0 / 006046f0 / 00604670) w-hudrest: HUD STYLE ORIGINAL draws the rest of the Free Ride layer: `OriginalFreeRideHud` (replaces `OriginalDiscoveryCounter`) builds the install's own layers (`BuildLayerOwn`) and drives MXUICompassComponent from the core: the camera's heading, the scrolling CompassTile strip (8 tiles of 45 degrees, +0xfc = 8.0), N / E / S / W at 320 + wrapped angle x 7.111, icons clamped to +-45 degrees; in the discovery-mode Free Ride the ten challenge-type icons (one colour each of the CompassChallenge glyph) at the level's StartActivity triggers, alpha (2500 ft)^2 fade; the EventTimerComponent. New read export `rx_session_compass` (game/rx_compass_api.h: the Free Ride's area list A+0xff190, TargetPractice's targets A+0xff3b4, the "Trailblazer_Finish_Trigger" entity; memory reads only; `core/tools/compass_native_test.c`). Challenge pages as the InGamePageGraph has them: HillClimb (timer on DurationWatermark, PositionWatermark + HeightAchieved "n ft", Distance label), Trailblazer (timer counting up, compass with the FinishLine flag and the distance label), Target Practice (timer, compass with the unhit targets, Target Hit Component "n/m" for 3 s); BigAir / Pathfinder / StepUp have no page of their own and show the Free Ride's. Hidden by a menu, MODERN and H. Not drawn: StuntFeedback, Arrows, Flooding and the camera label of those layers (other work: the stunt names and the wreck arrows have their own paths). |
| F10 | Online / party HUDs: Tag, Snake, CTF, Greed, Outbreak, Spectator, Network and Voice overlays | SinglePlayerTag/Snake/CTF/Greed/Outbreak, SpectatorOverlay, NetworkOverlay, VoiceChatOverlay | DEFERRED | party modes + online. SinglePlayerOutRunner is the career Outrun HUD (DONE) |
| F11 | Cameras: 4 driving presets (DRIVING CAMERA), first person, camera switch key, scenario cameras | OptionsFrontEnd DrivingCamera; CameraSwitch action; Cameras/*.xml | DONE | `rx_session_set_driving_camera`; OPTIONS > CAMERA & HUD; client's Dynamic FPV (EXTRA) |
| F12 | Replay cameras and the end-of-race replay (finish clip, highlight clips) | MXReplayCamera / Tower / Animated; ClipRecorder (replay.md) | DONE | exhibition and career replay presenter (`OwnReplayRenderer`, `rx_session_replay_*`, w-replay): plays behind the results, camera + fades. Recorded NAT_Stillwater (arcade probe); other types headless |
| F13 | Results page: table with TOTAL / PENALTY / BEST LAP, projected times of unfinished riders | PostEventResults (PopulateResultsTable) | DONE | `OwnRaceResults` for every offline mode with a results page (w-pausemodes): the lap races (National, Supercross, Terracross, Powercross; TOTAL, PENALTY, BEST LAP, w-resultsrng's projected times), Waypoint (TOTAL TIME), Freestyle (TOTAL SCORE, BEST TRICK SCORE, BEST TRICK per rider + the judges' final card), Section Race (SECTIONS, BEST SECTION, BEST SECTION TIME), in the career look, pad-drivable; the Waypoint / Freestyle / Section Race values come from the new read-only `rx_session_mode_results`. Since w-wpfill the core runs every offline mode's original filler where the original's page starts (Waypoint 00615450 with the projected total 00613a30, Freestyle 00614bf0, Section Race 006152e0, the Eliminator's place column 005dc7a0: verified per call in oracle boots that reach each mode's results page) and `OwnRaceResults` shows the table's values (the Waypoint's projected TOTAL for a rider still racing, the game's score / sections text, the ELIMINATED place with the install's STR_FIRST..STR_FOURTH by the original's rule); the Eliminator's results page is the career's (its own panels) where `OwnRaceResults` is not used; real-player runs of the results pages: Freestyle and Section Race by their own clocks; Waypoint / lap races not finished in a real run (showcase only) |
| F14 | Post-event menu: CONTINUE, HIGHLIGHT REEL, RESTART, MAIN MENU | PostEventMenu.lua | DONE | results page (`OwnRaceResults`): HIGHLIGHT REEL entry between RACE AGAIN and RETURN TO MENU when the world's reel plays (EventHasReplay); it hides the table so only the reel and BACK show (PostEventHighlight: the PC reel has no other controls). The career's results page (`OwnCareerPanels`, w-partials) has it under CONTINUE when the world's replay renderer exists (`OwnCareerRuntimeContext.ReplayAvailable`; the PostEventMenu offers it in tour mode too): the page's elements are hidden, not rebuilt (its epoch and CONTINUE stay valid), only BACK is on the legend, B / BACK returns to the entry  |
| F15 | Finish scenario and podium with trophy girl, crew chiefs | RaceFinishScenario, FMXFinishScenario | DONE | `OriginalPodium`, w-cast, w-npcs |
| F16 | Wreck, ragdoll, remount, reset to track / custom reset markers, reverse, clutch, emergency brake; the wreck-recovery (crash) prompt: flick the stick the right way within the window to save a bad landing | KeyBind actions ResetPlayer, CustomResetPlaceMarker / GoToMarker, Reverse, Clutch, EmergencyBrake; MXUIReflexComponent "Arrows" (HUD layers), WreckAvoidanceParms.xml, Player_UpdateWreckRecovery 0054ea30 | DONE | core (game logic); the prompt (was: no HUD, the original's arrows hidden) is `rx_session_reflex_hud` + `OriginalHud.Reflex.cs` (the layer's own Arrows, ORIGINAL HUD) + `ReflexHudView.Reflex.cs` (MODERN: arrow, countdown ring, control glyph, RECOVERED / TOO LATE), w-reflexarrows; headless `core/tools/reflex_hud_test.c`, gallery `hud-reflex-*` / `hud-original-reflex`, real ride `moto_career_flow/run.py --reflex-arrows` |
| F17 | Ground and aerial stunts, trick names, Freestyle judging, FMX | StuntTable, FMXJudgingParameters.xml | DONE | core + `RxStuntHud`, `ReflexHudView.Modes` |
| F18 | Mud, roost, track deformation, water, particles | Terrain deform, MudManager | DONE | core + Unity renderers; known gaps in sec. 10 |
| F19 | Rumble / vibration | Vibration setting | DONE | `ReflexRumbleScale` |
| F20 | Free Ride activation areas: AAA notification, challenge start by the pause key, Pre / Post challenge popups, medals, payout, EXIT to Free Ride | MXUIManager_OnTriggerEnter, SinglePlayer_AAA_Activation.lyr, PreChallengePopup, PostChallengePopup, AAALoading | DONE | `OwnActivationArea`, `OwnPreChallenge`, `OwnPostChallenge`, `OwnAaaLoading`; recorded `--activation` run PASS (Hill Climb; Trailblazer / Target Practice formatted, not run in Unity per QUEUE) |
| F21 | In-level video screens, ads, crowd, camera flashes, NPCs | jumbotron / projector Bink screens, crowd entities | DONE | w-screens, w-npcs (needs ffmpeg on the machine) |
| F22 | Rewind / flashback, replay save and export, photo mode | none in the PC game (STR table has only STR_REPLAY_*; no photo strings) | N/A | not original features |

## 8. Content: event types, levels, vehicles, DLC

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| G01 | Vehicles: MX Lite, MX, ATV, UTV, Super Buggy, Pro Lite truck, Pro 2 truck | VehicleType (7), VehicleModel (71) | DONE | `ReflexEvents.DrawableVehicleTypes`; career runs per vehicle type (w-career, w-vfactory) |
| G02 | Levels (41 incl. 6 DLC) | EventLocation | DONE | all events run headless (STOP_AUDIT M1); unreachable dev levels (SX_Fresno, *_LD, NAT_DLC_Test01, Ctrl_*) are not offered, as the original |
| G03 | DLC: vehicles (ATVDLC01, MXDLC01 + graphic kits), rider gear (RiderDLC01 / 02), levels Dalton, FreightYard, FunFactory (+B) | MXTables_DLC003/4/5, MXUI_DLC00x, found by file glob | DONE | merged tables (`ReflexEvents.DlcDatabases`), DLC pills in browser and garage; the original's DLC popups (bind error, missing, notification) belong to its validation / online paths (notification and missing calls are commented out in MainMenu.lua): N/A |
| G04 | Event types with no rows on PC: Monster Smash, Hockey, Opencross, Supermoto, Minimoto, Endurocross, UTVMoto, FMXJam | EventType rows without EventTypeLocations | N/A | restored-content candidates (memory: restored-content-pass) |
| G05 | Challenges with spec fields but no rows: Big Air, Pathfinder, Step Up, Jumper, Stoppie, Wheelie | PlayerChallenges / MXActivity*Challenge | N/A | classes ported (w-freestyle); restored-content candidates |
| G06 | Party modes Tag, Snake, Capture, EggHunt, Infection, Graffiti, PokerRace | MXActivity* (online only on PC) | DEFERRED | online framework |
| G07 | Section Race (arcade 5 + career 5 series) | MXActivitySectionRace | DONE | arcade rows + career series via w-career |
| G08 | Freestyle (FMX) arcade and career | MXActivityFreestyle | DONE | w-freestyle |

## 9. Audio

| ID | Feature | ORIGINAL | PORT STATUS | NOTES |
|---|---|---|---|---|
| H01 | Engine sounds per engine size, AI engine mix | FMOD events | DONE | w-audio / w-sfx (AIFX slots) |
| H02 | Surface, skid, landing, collisions, wind, rider vocals | AudioCollisionManager, foley | DONE | w-sfx (`OriginalGameSound`) |
| H03 | Crowd, announcer / activity cues, level ambience, in-race music (scores + jukebox) | CrowdSoundManager, ActivityAudioLogic, MusicPlayer | DONE | w-sfx. Left: reverb zones, first-person reverb, programmer-selected sound picks, rider vocal at the pelvis |
| H04 | Front-end music and UI sounds | UI_MUS, UI_02 | DONE | A07 |
| H05 | Challenge / activation narration | AudioLocalizedActivity | DONE | `OwnActivationArea` voice |

## 10. Presentation (summary only; rendering is tracked by its own agents)

Open items recorded in QUEUE at 85e1684: motion blur and depth of field (no pass, graphics.cfg flag only); camera-flash quads (done by w-screens);
ocean random phases differ from the game's seed and the wake gets no splats; reverb zones; Enhanced lighting is an option (LIGHTING ORIGINAL /
ENHANCED, approved and merged); Windows builds need ffmpeg bundled for videos. None of these is a gameplay feature.

## 11. Tutorials, online, social

| ID | Feature | ORIGINAL (evidence) | PORT STATUS | NOTES |
|---|---|---|---|---|
| I01 | MotoSkills / "MX Training" lessons 1-6 (lean, hold tight, turn, jump, wreck save, tricks) with video, narration, pause menu, locks | MainMenu legend TUTORIALS -> Tutorial Load Page; Tutorial Pause Menu Page; TutorialManager; Tutorial1-6.bik | DONE | MOTOSKILLS hub entry (`OpenShell.Tutorials.cs`, w-tutorials, merged): lessons with videos, narration, chevron shader, tutorial pause and popups; the lead confirmed it works in play (2026-10-28) |
| I02 | Tutorial first-time entry, introduction, pre / post / failed / conclusion popups | Tutorial*Popup.lua | DONE | MOTOSKILLS hub entry (`OpenShell.Tutorials.cs`, w-tutorials, merged): lessons with videos, narration, chevron shader, tutorial pause and popups; the lead confirmed it works in play (2026-10-28) |
| J01 | Online Mode Menu: playlists, quick / ranked matches, sessions, invites | Online Mode Menu (OnlinePlaylist.lua), Network Session Search (LAN), Online Invite, Session* popups, PlaylistData.xml | DEFERRED | future online framework |
| J02 | Online leaderboards, friends lists, voice chat, spectator | MotocardRecords online half, PS3FriendsPopup, VoiceChatOverlay, SpectatorOverlay | DEFERRED | offline half: D13 |
| J03 | Network player names option | OnlinePlayerName | DONE | GAMEPLAY > ONLINE PLAYER NAMES (stored, no effect offline) |

## 12. Port-only extras (not in the original)

Modern HUD with per-mode widgets and HUD STYLE switch; LIGHTING ENHANCED (shadows, AO, HDRI skies); 3D menu world UI_Compound with the push-bike
walk-in; drivable hovercraft on FR_Alpine; bike model mods (`Mods/Bikes`) and engine sound packs (Steeler's five + `Mods/EngineSounds`) chosen per
bike in the garage; GAME LIBRARY (install connect, world cache, rebuild); crash prompt and redacted support report; save backups and restore;
UNLOCK EVERYTHING profile; favourites / recents / CONTINUE / QUICK START; Dynamic FPV camera; mouse lean; rumble strength; UI scale, reduce motion,
controller glyph styles; Windows player; project credits.

## 13. Summary counts

109 feature rows (106 original features + 3 port-only rows kept in the tables; the longer extras list is sec. 12).

| Area | DONE | PARTIAL | MISSING | DEFERRED (lead) | IN PROGRESS | N/A (not on PC) | EXTRA | rows |
|---|---|---|---|---|---|---|---|---|
| A Shell, startup, navigation | 8 | 0 | 0 | 3 | 0 | 4 | 0 | 15 |
| B Arcade (Race, Free Ride) | 10 | 0 | 0 | 1 | 0 | 1 | 1 | 13 |
| C Career | 14 | 0 | 0 | 1 | 0 | 1 | 0 | 16 |
| D Garage, Motocard | 14 | 0 | 0 | 1 | 0 | 1 | 0 | 16 |
| E Options, controls | 5 | 0 | 0 | 0 | 0 | 2 | 2 | 9 |
| F In-race | 20 | 0 | 0 | 1 | 0 | 1 | 0 | 22 |
| G Content | 5 | 0 | 0 | 1 | 0 | 2 | 0 | 8 |
| H Audio | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| I Tutorials | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| J Online, social | 1 | 0 | 0 | 2 | 0 | 0 | 0 | 3 |
| **Total** | **84** | **0** | **0** | **10** | **0** | **12** | **3** | **109** |

Recounted from the rows, 2026-10-28 (after w-startrace; C03 PARTIAL -> DONE; I01 I02 DONE: MotoSkills merged and confirmed in play): of the 94 rows that are real, reachable-on-PC features (109 less 12 N/A less 3 EXTRA), **84 are DONE (89 %), 0 PARTIAL, 0 MISSING** and 10 DEFERRED by the lead (restored content / championships, online, split screen, own cheats, profile slots, leaderboards; B07 arcade fully open).
Earlier counts: (before w-startrace: 83 DONE, 1 PARTIAL = C03.) Of the 94 rows that are real, reachable-on-PC features (109 less 12 N/A less 3 EXTRA), 67 are DONE (71 %), 11 PARTIAL, 4 MISSING, 10 DEFERRED by the lead
(B07, decided 2026-10-28, is counted there: arcade stays fully open) and 2 in progress (tutorials). Counting the DEFERRED and IN PROGRESS rows as "not this wave", the open work is **15 rows: 4 MISSING + 11 PARTIAL**. (w-partials, 2026-10-28: D14 F02 F14 PARTIAL -> DONE, B07 MISSING -> DECIDED.) (w-motocard, 2026-10-27: D11-D13 DONE, D14 PARTIAL; w-pausemodes, 2026-10-27: F01 F09 F13 -> DONE, F02 MISSING -> PARTIAL.)

What the numbers hide: the Motocard pages (D11-D14) are where a player will now notice the port is not finished. The arcade and Free Ride modes have their own pause (with the Free Ride's
vehicle list, tuning and map) and results pages with a pad (F01, F02, F13, w-pausemodes); what is left of them is the projected Waypoint totals (the pro riders in the vehicle list and the career's HIGHLIGHT REEL item were closed by w-partials).

## 14. Prioritised list of what to build next

Ordered by what a player hits first in modes the shell already offers, then by size. "Core" = needs functions ported into `core/` (or a new
read-only export); "Unity" = presentation work only.

### P1: holes in modes the shell already offers

| # | Build | Rows | Difficulty | Core ports? |
|---|---|---|---|---|
| 1 | **DONE (w-pausemodes, w-wpfill: the fillers; QUEUE)** Own pause and results for every mode, with a pad. Widen `OwnRacePause.Supports` and `OwnRaceResultsPolicy` (now activity types 0 and 1 only) to Terracross, Powercross, Waypoint, Freestyle, Section Race, Free Ride and the challenge worlds; give the fallback `OriginalPauseMenu` pad input or retire it. `rx_session_results_page` is already generic for the race activities | F01 F13 F14 | M | none for Terracross / Powercross; optional S each for the Waypoint / Freestyle results-table fillers (RNG parity, QUEUE w-resultsrng "Left") |
| 2 | **DONE (w-pausemodes, w-partials: the pro riders in the vehicle list; other riders on the map: N/A offline)** Free Ride pause extras: VEHICLE LIST (quick swap = a new session with the chosen garage vehicle), VEHICLE TUNING (the garage's tuning rows, `rx_session_tuning_set`), the Free Ride map with rider arrows, no RESTART in Free Ride, Hit Discovery counter | F02 F09 | M | the discovery counter needs a small read-only export (S-M); the rest is Unity |
| 3 | **DONE (w-motocard)** **Motocard pages**: stats + career completion, Highlights (48), offline Records, Stunts list. Data is in the player snapshot (PlayerRider, PlayerMinedStat, PlayerRecords, PlayerHighlight); career completion (App.CalculateCareerCompletion 00743870) is table maths: reimplement in C# or port it | D11 D12 D13 D14 | M-L | optional S (00743870); otherwise Unity only |
| 4 | **Tutorials (MotoSkills)** | I01 I02 | in progress | being done in `REFLEX-w-tutorials` |

### P2: original flows the open shell dropped

| # | Build | Rows | Difficulty | Core ports? |
|---|---|---|---|---|
| 5 | **DONE (w-p2front, QUEUE)** Free Ride challenges page (per-level challenge list with medals, thresholds and a direct start) | C13 | S-M | none |
| 6 | **DECIDED (lead, 2026-10-28): arcade stays fully open; not built.** Arcade unlock progression (locations and AI levels locked until the career unlocks them) | B07 | S | none (read PlayerProductsUnlocked) |
| 7 | **DONE (w-p2front, QUEUE; the StartRace popup C03 followed in w-startrace)** Career polish: Career Welcome (difficulty), Career Completion (video), vehicle-requirement and locked-reason popups with a quick vehicle swap, credits in the hub | C03 C08 C09 C10 C11 C12 | S each | none; C09 needs the completion % (item 3) |
| 8 | **DONE (w-p2scene, QUEUE)** Gate selection with the camera on the gates | C04 | S-M | read-only export `rx_session_start_gates`; Unity does the camera (the original's own formula) and the cards |
| 9 | **DONE (w-p2scene, QUEUE)** Event Intro title card, intro skip (Enter / A through `rx_session_skip_scenario`; the pad's X is the game's own); no outro hint: the PC original has no outro skip | F05 F04 | S-M | the skip setter was added (tiny) |
| 10 | **DONE (w-p2scene, QUEUE)** Vehicle Select 3D preview in the arcade setup (the menu world's hero spot) | B02 | S-M | none |
| 11 | **DONE (w-p2front; w-partials: F14 on the career's results page too)** Small ones: quit confirmation, arcade RULES popup, HIGHLIGHT REEL entry on the results page, first-run Motocard welcome, screen-adjustment page | A04 B08 F14 A11 E06 | XS-S each | none |

### P3: restored-content and new frameworks (lead-scheduled, not gaps)

| # | Build | Rows | Difficulty | Core ports? |
|---|---|---|---|---|
| 12 | Championship weekends, then the unreachable event types and challenges (Opencross, Supermoto, Minimoto, Endurocross, Monster Smash, Hockey, Big Air ... Wheelie) | C15 G04 G05 | L-XL | classes already ported and verified (w-champ, w-freestyle); needs authored series rows, menu path, intro cast setup (small core for the cast) |
| 13 | Online framework: party modes, online leaderboards, playlists, voice, spectator | G06 J01 J02 F10 | XL | a new network layer (Open Unleashed's), then the party-mode classes are already in the exe |
| 14 | Split screen (not reachable on PC, so a port addition) | B13 | XL | yes: a second human, viewports (00757660 3-viewport branch), second pad |
| 15 | The lead's own cheats | D16 | S-M | none |
| 16 | Multiple profile slots (not in the PC original) | A10 | M | none (PlayerSnapshotStore per slot) |

## Appendix: coverage of the original page graphs

Every page of the exported graphs, grouped; the IDs refer to the tables above.

- FrontEndPageGraph (50): StartGamePage, Title Page, Attract Page, Load Game Page, Splitscreen Load Game Page: A01 A02 A09; Main Menu Page: A03; Event Select Page B01, Vehicle
  Select Page B02, Player2 Vehicle Select B13, Locations Page B03-B08, Split Screen Controller Select Page B13, Multiplayer Select Page / Online Mode Menu / Network Session Search
  Page / Online Invite Page J01; Race and Ride Page C01 C12, Race and Ride Events C01-C04 C10 C11, Race and Ride Challenges Page C13, Career Welcome Page C08, Career Completion Page C09;
  Garage Page D01 D10, Garage Customize Rider Page D02 D03, Garage My Rides Page D05, Garage Customize Vehicle Page D06; Motocard Page D11, Motocard Highlights Page D12, Motocard
  Leaderboard Page D13, Motocard Stunt Page D14, Online Motocard Page J02; Options Page E01, Options Controller Page E03, Controller Remap Page E02, Credits Page A15; Autosave Notice,
  Motocard Setup Name / Welcome / Vehicle / Gear / Skintone / Jersey Name / Jersey Number / Completion, Screen Adjustment Page A11 E06; Dev Menu Page, LoadLevelPage, Join Network Game A14;
  Demo Splash / Video / Event / Event Splash Pages A13.
- InGamePageGraph (39): Paused Menu Page F01 F02, Tutorial Pause Menu Page I01, Fullscreen HUB (router), the 22 single-player HUD pages F08 F09 F10, the 6 split-screen HUD pages B13,
  Event Intro Page F05, Event Rider Page (no sender on PC: N/A), Event Outro Page F05, MXUIAAALoading Page F20, Gate Selection Page C04, Vehicle Loading Page F02, PostEventResults
  F13, PostEventPage F14, Post Event Highlight Page F14, Post Event Reward Page C06, Post Event Moto Results Page C05.
- TransitionPageGraph (11): loading pages A05, Profile Login Status Updated Page A09, Vehicle Reward Page C07, Test Drive Exit Page D10, Controller Layout Page A13.
- Popups and overlays (PopupPageStack, OverlayPageGraph): confirm / purchase / insufficient funds D02 D05; Practice, Start Race, Rules, Locked, Invalid Race Requirements C03 C10 C11;
  Pre / Post Challenge F20; Tutorial popups I02; Cheat popups D16; Session / Network / Storage / Profile / PS3 popups A09 J01 (N/A or DEFERRED); DLC popups G03.

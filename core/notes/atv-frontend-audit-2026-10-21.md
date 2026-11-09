# ATV frontend route and compatibility audit

Read-only source audit while central native builds were frozen. No UI/player run or frontend edits performed.

## Current bundled menu (actual windowed tests)

The current bundled menu uses **main Free Ride → one of three location cards → setup → Start Ride**. Parent windowed tests confirmed the saved ATV selection is preserved through this actual route. Use it for the current product test. The older source route below is a legacy frontend audit; its selection-reset caveat does not describe the bundled menu.

## Legacy frontend source route

From a saved ATV garage profile, return to the main menu and use **Arcade → Free Ride → rider → ATV vehicle type → ATV model → location → Accept**. Enter/A accepts and arrows/d-pad navigate (FrontEndManager.cs:449/460). Free Ride is the first activity poster (FrontEndScripts.cs:216); its accept opens Vehicle Select, whose three accepts advance rider→type→vehicle→location (FrontEndVehicleSelect.cs:337/352/370). Location Accept calls LaunchEvent with the selected Garage snapshot (FrontEndScripts.cs:424). Free Ride disables rules and uses one rider/no AI (FrontEndScripts.cs:334/484). A suitable unlocked location may be chosen normally; no smoke shortcut or MX selection should be used for the ATV test.

Two route caveats:

- Garage **TEST DRIVE** is explicitly unavailable on the own-core backend: ReflexGarage.PluginHasTestDrive returns false when UseOpenReflex (Content/ReflexGarage.cs:51), and GarageHub.OnStartPressed plays Locked/returns (FrontEndGarage.cs:167). It would otherwise launch UI_Compound FreeRide, so it is not the ordinary own-core test route today.
- Saved equipped ATV is initially resolved in VehicleSelect.Start, but its final OnRiderListFocusedChanged call (FrontEndVehicleSelect.cs:135) rebuilds the type list and resets its selection to index0 (line231); vehicle list also resets index0. Manually confirm **ATV** and the intended model before advancing. This is a selection/persistence pitfall, not an ATV engine lock. No fix made during the audit.

## ATV input and presentation

No additional MX-only compatibility gate found in the audited frontend/bridge paths. ReflexVehicles.Drawable includes FactoryAtv; ReflexAtv type4 resolves FactoryName Atv, and its model builder uses ATV_Hierarchy and actual part packages. ReflexBikeModel.cs:149 deliberately creates front/rear axle holders for the ATV while its visible parts follow OriginalBikePose nodes, rather than requiring MX tire model names. OpenReflexResultsAdapter.CopyVehicle copies24 position/orientation floats (six bodies); ReflexVehicleInfo.Front/Rear average the correct two wheel bodies per axle. OriginalWorldRuntime applies original node poses/flags and uses four-wheel axle information for its camera path.

Driving keyboard states are forwarded through DirectInputMap and pads as raw XInput ranges (OriginalWorldRuntime.cs:555..640); OriginalInput delivers mouse/rider-lean and user remapping through native settings. These paths have no ATV-specific input suppression. Garage selection is captured in OpenGarageSnapshot and handed to CreateOpen (OriginalWorldRuntime.cs:276). This source review establishes a viable UI route; actual visibility, input response, physics and screenshots still require the parent's windowed ATV ride.

## Native motion-input triage

The public vehicle fixture's DIK W0x11 is the genuine throttle. Read-only decoding of `builds/sol_core_features/vehicle-profiles/atv.bin` confirms ControllerPresetId1 and no ControllerRemapping cells. The supported garage snapshot has only five player tables; default keymap construction reads remap keyboard Throttle0x39 (W) and installs it independently of preset/handedness/motion (`build_activity_controller.c`). `bike_read_controller_input` always reads the same throttle compound map into S+1c; presets affect steering/reflex routing. The original oracle harness `native/drafts/world_runtime/boot_main.cpp:153..166` uses exactly the same W/S/A/D DIKs, with arrows for rider lean, under REFLEX_DRIVE2.

The fourth native motion sample reached racing frame773 with six bodies/50 nodes, but RPM remained1500 and maximum speed0.599 (`builds/sol_atv/logs/vehicle-fourth-motion-atv.log`). This does not identify a physics error. The next read-only discriminator is raw throttle S+1c versus processed engine throttle S+5880, with S+4 control type and the inherited pre-step gates: listener L=V+90 control state L+1f0 (V+280), S+2de0 /2de4, player capture U8(p+21c), controller p+1f0 and globals a7a994/a9418c. The original logic deliberately clears inputs when those gates fail (`bike_vehicle.c:55..71`). No synthetic input-block or velocity writes are justified by these samples.


The no-motion triage above was subsequently resolved: the native WreckChecker called ATV wheel-load/grip virtuals using a C float return prototype although those ports return rx_f80. Correct typed dispatch fixed the zero threshold/forced-wreck behavior. W was correctly bound throughout. The parent documents the native ride acceptance; the earlier low-speed sample is historical failure evidence, not the current result.

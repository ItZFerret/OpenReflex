# Cut / unused content (audit 2026-11-02, modes excluded)

Read-only audit of the PC game files, tables, front-end Lua and executable strings. Cut modes and the championship weekends are in
FEATURE_AUDIT.md; the restored tracks (Fresno A / B) in QUEUE.md (w-fresnob, w-fresnolook).

## Restorable now (data complete; a flag or table rows)
- **REFLEX SPECIAL SERIES**: store item 2929 (Hidden=1), Series 84-88, chained by SeriesRewardsUnlock 84->85->86->87:
  Sugar Ridge MX (Eliminator, NAT_Concord), Watershed (Rival, FR_Alpine), Stonepoint R1 (Section Race, SX_Springfield), Iron Gate
  (Overtake, TRX_Lynchburg), Barranca del Diablo (Outrun, TRX_Verde). Nothing on PC unlocks it (CheatCode tables empty).
- **Bike parts with no VehicleModelParts / AssetPackage rows**: Neptune-F graphic kits Rainbow Blue / Rainbow Yellow / THQ Red / THQ White
  (hi-res too); handlebars MX_Hbar_Universal_Duece / _Solex (Brand_Universal icon 1004 exists, its package does not); MX_Exh_ProCircuit_T4
  finish; VehiclePart 30 / 32 "Aluminum" Pro Circuit 125 / 250 (rows only, default material).
- **Rider gear**: No Fear Spectrum Black jersey (RiderDLC02_jersey_NoFearSpectrum_Black, unique colour map, hi-res), O'Neal Clear chest
  protector (ChestProtector_Oneal_Clear), Red Bull Bell Moto-8 helmet (RiderGear 644 / store 3505 Hidden=1; Stewart's pro gear).
- **UI_Riders announcer bank** (UIAudio UI_Riders.ENG / .FRE): CARMICHAEL, GRANT, MCGRATH, STEWART, VILLOPOTO, MY_RIDE; nothing plays it.

## Restorable with work
- **Pro 4 truck** (VehicleType 13, STR_VEHICLE_PRO4TRUCK, Pro4_Hierarchy row 19): Pro401 / Pro402 meshes, collision, wheels, tyres,
  grey kits; untextured prototype (body borrows Pro 2 textures); no physics (0044f690 knows truck2WD / truck2WDlite only), no audio, no
  hi-res garage model, hierarchy differs from the shipped trucks (no NDE_RiderLocator / DMG_*). Route: Pro 2 physics + Pro2V8 audio.
- **16 mirrored stunts** (*_R_Air) in the stunt table, input mappings commented out in AerialStuntInputMappings.xml; mirrored playback unverified.
- 32 AI pros with PlayerSelectable=0 (a new feature rather than a restoration).

## Reference only
- VehicleType rows with no data: MX Mini (MX50cc), Monster Truck, Trophy Truck, Offroad Buggy, Sandrail, Golf Cart, ORV, ATV / MX Freestyle.
- Physics/RCcar_vehicle.bxml, RCTruck_vehicle.bxml (no models); Racelines bridgeportsxmxrl / sutherlinnatmxrl (no tracks).
- SCMG_ANIMALS wildlife scenarios in every level (no animal models or anims).
- Unused strings (tuning / spec screen, gear sets, replay controls, showroom), DLC001 "Arenagrid Freeride" / MG_Level1, HUDFlag*.bik.
- Levels: Shelby (SM / UTVM, loading pictures only), FMX_BuenaVistaC, FMX_Kingston (names only); see QUEUE / the Fresno sections.

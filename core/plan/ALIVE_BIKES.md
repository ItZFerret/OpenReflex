# MX vs ATV Alive bikes and ATVs

Status: **experimental** (branch `experimental`, 2026-11-09). The in-client Alive importer (GAME LIBRARY > MX vs ATV ALIVE) imports Alive's
motorcycles next to its levels: each one becomes a `.rxmod` bike folder (`core/plan/RXMOD.md`) in the client's content folder,
`<content>/AliveBikes/<slug>/`, made on the player's machine from their own disc image and LIVE packages. Nothing of Alive ships with the
game; an imported bike is the player's own, like an imported level.

Code: `Unity/Reflex/Assets/Scripts/Content/AliveImport/AliveBikes.cs` (tables, bike list, conversion, install),
`AliveImport/AliveBikeMaps.cs` (texture decode, the bakes, a deterministic PNG writer), `OpenFrontEnd/OpenShell.AliveImport.cs` (the
ALIVE BIKES rows of the import list, the results), `Content/BikeMods.cs` (`AliveFolder`: scanned next to `Mods/Bikes`, mod ids
`AliveBikes/<slug>`). Tests: `tests/alive_bikes/run.sh` (the importer outside Unity, every bike converted and checked),
`tests/menus_showcase` page `alive-bikes` (the real screens: scan, list, import, results, BikeMods) and `rxmod-garage` (the MOD cards),
`tests/rxmod/rxmod_ride.py` (a race on one).

## What Alive ships (survey)

Sources: the disc (`MX vs. ATV Alive (USA, Europe).iso`) and the title's content folder `5451087B/00000002`: 4 level packs and 10 more
LIVE packages: **Honda Vehicle Pack**, **Yamaha Vehicle Pack**, **KTM Vehicle Pack 1 / 2**, **Suzuki Vehicle Pack 1 / 2** (each brand's MX
bikes and an ATV), **Rider Gear Pack 1 / 2 / 3** (rider gear and graphic kits), **MotoClub Pro** (tables only). Extracted for study under
`_re/out/alive_dlc/<pack>` (alive_stfs.py).

Every Alive motorcycle (the `VehicleModel` rows of `MXTables_Default` + `MXTables_DLC*` whose `VehicleType` is a `MotocrossBike`; ATVs left
out):

| Alive # | bike | pack | VehicleType | physics file | chassis package | rides as (Reflex #) |
|---:|---|---|---|---|---|---|
| 23 | Blaze 450F | disc | MX450 | mx450f | MX / MX_MX1 (MX_NeptuneF) | Blaze 450F (1) |
| 24 | Blaze 250F | disc | MX250 | mx250f | MX_MX2 (NeptuneF_Lite) | Blaze 250F (11) |
| 28 | Blaze 125 | disc | MX125 | mx125 | MX_MX6 (Neptune_Lite) | Blaze 125S (12) |
| 26 | Takashi 450F | disc | MX450 | mx450f | MX_MX4 (EarthF) | Takashi 450F (32) |
| 27 | Takashi 250F | disc | MX250 | mx250f | MX_MX5 (EarthF_Lite) | Takashi 250F (31) |
| 25 | Takashi 125 | disc | MX125 | mx125 | MX_MX3 (Earth_Lite) | Takashi 125R (29) |
| 35 / 36 / 41 | Honda CR125R / CRF250R / CRF450R | Honda Vehicle Pack | MX125 / MX250 / MX450 | class files | MXHonDLC1_MX1..3 | Blaze 125S (12) / 250F (11) / 450F (1) |
| 33 / 34 / 43 | Yamaha YZ125 / YZ250F / YZ450F | Yamaha Vehicle Pack | same | class files | MXYamDLC1 | Takashi 125R (29) / 250F (31) / 450F (32) |
| 31 / 32 | Suzuki RM125 / RMZ250 | Suzuki Vehicle Pack 1 | MX125 / MX250 | class files | MXSUZLDLC1 | Shima 125R (6) / 250F (17) |
| 49 | Suzuki RMZ450 | Suzuki Vehicle Pack 2 | MX450 | mx450f | MXSUZDLC2 | Shima 450F (10) |
| 39 / 40 | KTM SX150 / 250SXF | KTM Vehicle Pack 1 | MX125 / MX250 | class files | MXKTMLDLC1 | KTM 125 SX (36) / 250 SX-F (18) |
| 47 | KTM 350SXF | KTM Vehicle Pack 2 | MX450 | mx450f | MXKTMDLC2 | KTM 450 SX-F (28) |
| 51 | MX250 FMX (the Sliderock freestyle 250) | Sliderock Canyon | MX250 | **mx250** (2-stroke) | MX250FMX_MX1 | Dakumoto 250S (16): the brand no other Alive bike takes |
| 30 | **50cc mini** | Stewart Compound | MX50 | **mx50** | MX50DLC1_50 (MX_FR50, MX_Hierarchy_50) | none: its own class (below) |
| 22 | **MXFR650** (the enduro-style big bore; AllowAI 0) | disc | MX650 | **mx650f** | MXFR_MXFR650 (MX_EarthF_650) | none: its own class (below) |

Alive's base bikes reuse Reflex's fictional chassis names (NeptuneF, EarthF ...) but are reworked models (other surface / vertex counts).
The graphic kits: 121 on the disc, 10-31 per brand pack, 126 in the Rider Gear packs (only each bike's default kit is imported today).

## The ATVs (2026-11-09): every Alive vehicle imported

Alive's VehicleModel rows are motorcycles and ATVs only (no trucks, buggies or UTVs): 21 motorcycles + 9 ATVs = **30 vehicles, all
imported**. The importer finds ATVs by VehicleType FactoryName "Atv". Alive's `ATV_Hierarchy` is node for node Reflex's (50 nodes, the
same rest positions within 0.001 ft but the hand locators), so an Alive ATV rides as the Reflex ATV of its brand (one Reflex ATV class:
`atv_vehicle`) drawn on its own nodes (`"fit": "nodes"`: unscaled, bound to its own rest pose; `BikeMods.ModsFor` / `ApplyRx` now take
an ATV for such a bike), its hands and feet on its own grips / pegs.

| Alive # | ATV | pack | physics file | rides as (Reflex #) |
|---:|---|---|---|---|
| 13 / 15 | Blaze ATV 250F / ATV 450F | disc | atv250f / atv450f | Blaze ATV (0) |
| 12 / 14 | Takashi ATV 250F / ATV 450F | disc | atv250f / atv450f | Takashi ATV (21) |
| 42 | Honda TRX450R | Honda Vehicle Pack | atv450f | Blaze ATV (0) |
| 44 | Yamaha YFZ450 | Yamaha Vehicle Pack | atv450f | Takashi ATV (21) |
| 48 | KTM 450SX | KTM Vehicle Pack 2 | atv450f | KTM 450 SX ATV (74) |
| 50 | Suzuki LT450R | Suzuki Vehicle Pack 2 | atv450f | Shima ATV (20) |
| 29 | **ATV 1000 Dune** (the sand quad; product "DO NOT LOCALIZE") | disc | **atvDune1000** | own class 11 in the Raven ATV's slot (23) |

The ATV 1000 has no Reflex equivalent (a 2.3 ft swingarm against 1.7, its own `ATVFR_Hierarchy`), so it follows the 50cc: Alive's
`atvDune1000_vehicle` (93 of the ATV table's 103 records) over the ATV class file for the local rider (`rx_session_vehicle_physics` class
11: the core now takes ATV files, root type "atv"), its 49 hierarchy nodes' rest pose (`_nodes`), garage section "ATV 1000", leaderboard
board **`alive_atv1000`** (client, Worker `VEHICLES`: **not deployed**; the site shows unknown ids as they are). Native check
(`alive_class_probe`, Free Ride FR_Alpine, Raven ATV): stock 53.0 ft/s top, with the dune file 64.5 ft/s, class 11.

ATV colours: Alive colours ATV frames, swing arms, A-arms, rims and springs at run time from the parts' own table rows
(`VehiclePartCustomColor` -> `CustomColor.ConstantName` -> `CustomDynamicColor` RGB: the TRX450R's frame 0.90 grey, swing arm brown, A-arms
and rims black). The importer reads those rows for every vehicle and bakes `Template_ATV_FrameRefl_FNL` / `Template_Aarms_FNL` /
`Template_Vehicle_Wheel_FNL` / `Template_ATV_Spring_FNL` (a combo map's R times the colour) into diffuse maps.

Checks: all 30 through the real import screens (showcase `alive-bikes`), all 30 MOD cards (`rxmod-garage`, every shot opened full size);
races (`rxmod_ride.py`, NAT_Concord, 5 AI): Honda TRX450R PASS, ATV 1000 Dune PASS (its own physics in the race log).

## How Alive stores a bike

The same engine family as Reflex. A bike is its `VehicleModelParts` rows marked `Default`, each a `VehiclePart` naming a model and a
material package (`AssetPackageDatabase` -> package, database):

* **GraphicKits**: the model package is the **chassis** (`MXHonDLC1/MXHonDLC1_MX3`: the textures, surfaces, materials and models of the
  bike, `MX_Hierarchy`, `MXConvex`, and the physics file `mx450f_vehicle` as a bxml), the material package the **kit**
  (`MXHonDLC1_graphickit_MX3_Stock_Default`: the plastics material and maps).
* **Handlebars**, **Exhausts**, **Tires** (front + rear tyre models with rim / spokes / hub / tyre surfaces), **Wheels**, **Forks**
  (only fork stickers and tuning bxml), **Frame** / **Brakes** (tuning bxml), **EngineAudio**.
* `MX_Hierarchy` is node for node Reflex's (18 nodes: OBJ_Frame, OBJ_UPR_Forks, OBJ_LWR_Forks, OBJ_FRT_Tire, OBJ_Swingarm, OBJ_BCK_Tire,
  levers, NDE_ locators), so a full-size Alive bike fits a Reflex hierarchy exactly (CRF450R on the Blaze 450F: scale 1.0000, steering
  axis 0.00 in off, tyres 13.5 / 13.2 in against 13.5 / 13.3).
* Xbox resources (big-endian; tiled textures): the level importer's `Converters.Model` / `Converters.Surfaces` / `Textures.Convert` give
  the PC resources `Rfx.ParseModel` / `ParseSurfaces` read. Materials are read from Alive's own material documents (their sampler states
  carry ids the level materials never had).
* **Physics files** are Reflex's schema (the core reads `mx125 / mx250 / mx250f / mx450f_vehicle`, 108 records; Alive's add rider-air
  fields: mx50 shares 100 of Reflex's 108). Alive's brand bikes share their class's file.

### Alive's dynamic colours (baked)

Alive colours parts at run time (the Xenos programs, read with `_re/formats/xenos_disasm.py`):

* `Template_MX_ATV_DynPlastic_FNL` (plastics): the diffuse is grey; where its **alpha** is set the program draws `usr_userdyncolor`
  (0.4 + 0.6 (N.V)^1.5 of it), the combo map's **R** blends to the plate colour `usr_dynnumplate`, **G** is the mud mask, **B** the shine,
  the `Numbers` texture (the rider's number) on the second UV set. Baked: diffuse = the dyn colour where alpha, the plate colour where R
  (the facing value); SFX = (0, G, B) for Reflex's plastics program; `"numbers": true` keeps the rider's plate (RXMOD.md).
* `Template_MX_Wheel_Fnl` (rims, hubs): no diffuse; combo R the shading, B where `usr_mxrimdyncolor` x 0.92 tints; the hub
  `usr_mxhubdyncolor`. Baked into a diffuse per material.
* `Template_MX_FrameRefl_FNL` (frames): SFX R the shine, B flat: B := R for Reflex's frame program.

## The conversion (`AliveBikes.Convert`)

1. The chassis model's first LOD (static surfaces placed by their node's world matrix, skinned ones as stored, bones mapped by name), the
   part models on their nodes (a tyre model in its node's space on the chassis' tyre node; a bar / exhaust model in its node's space).
2. Each binding's material as the game resolves it: the first material of the bike's material packages (the kit first) whose
   `MaterialKey` prefixes the binding's; else the model package's own.
3. Its template by program: DynPlastic -> plastic, FrameRefl / Hbars / T_Diffuse -> frame, RavEngine / Exhst -> engine, DifSpecAlpha /
   AlphaMtl -> chain (alpha tested: the chain, the spokes), MX_Wheel -> wheel, TireBlur -> tire. Maps by name (`_SFX` / `Combo` the SFX,
   `Nrml` the normal map, the rest the diffuse; Mud_Noise, Numbers, the reflection cube, the blur strip and the fork sticker left out).
4. Written: `bike.rxmesh`, `maps/*.png` (19 per bike), `mod.json` (`"bike"` the Reflex VehicleModelId, brand / model from the store product,
   author "MX vs ATV Alive", `"fit": "axles"`). ~2.3 MB a bike, all 19 in about 4 s.
5. Installed: staged under `<content>/AliveBikes/.ab-*`, moved to `<content>/AliveBikes/<slug>`, with `<slug>.import.json` beside it (the
   files and their SHA-256; a folder without one is never replaced).

The PNGs are written by the importer's own deflate (fixed Huffman), not the runtime's: the same install gives byte-identical folders on
every machine, so two players who imported the same bike hold the same `.rxmod` content hash and see each other's bike online (the look's
`b:<hash>:<id>` token, RXMOD.md "Online"); a player without it sees the original bike.

## Status (2026-11-09)

* **Importer outside Unity** (`tests/alive_bikes/run.sh <scratch> --out DIR --bikes all <iso> <5451087B>`): 21 motorcycles found,
  19 converted (every one ok, 26-28 surfaces, 19 maps, 2.1-2.7 MB each), the 50cc and the MXFR650 listed as needing their own class.
* **In the client** (showcase `alive-bikes`, the Mono player, real screens): FIND LEVELS lists the levels, then ALIVE BIKES (19 of 21
  ticked, the two held back say why), IMPORT 19 BIKES, the results (19 installed), BikeMods lists all 19 as `AliveBikes/<slug>` riding as
  their Reflex bikes. The maps it writes are byte-identical to the .NET run's; the meshes differ only in the last float bits (the two
  runtimes' arithmetic) at identical sizes - a folder mod's content hash is over its entries' names and sizes, so the online token is the
  same on every machine.
* **Garage** (showcase `rxmod-garage`, all 19): each a MOD card under the bike it rides as ("CRF450R | HONDA · BY MX VS ATV ALIVE"), A
  equips it, the stage and the race setup's hero draw it; the rider's number on Alive's plates. Every shot opened full size.
* **Races** (`rxmod_ride.py`, one-lap National at NAT_Concord with 5 AI): Honda CRF450R on the Blaze 450F and KTM 350SXF on the KTM 450 SX-F
  (from `<content>/AliveBikes`): both PASS, finished; fit scale 1.0000, steering axis 0.00 in off the original's, 16 / 15 in of fork /
  swingarm travel on the mod's parts; the online look's token resolves to the bike (an unknown token: the original).
* Compile checks: Mac and Windows, 0 failed assemblies.

Known limits: only each bike's default graphic kit (Alive's other kits - 10-31 per brand pack, 126 in the Rider Gear packs - could be
generated as `.rxmod` skins); one LOD (the race model also in the garage: no HR twin); Alive's per-part custom colours are its defaults;
the bikes keep Reflex's engine sound of the bike they ride as.

## The 50cc and the MXFR650: their own classes (built 2026-11-09)

The user's choice (2026-11-09): both ride on **their own physics**, from Alive's own files in the player's install, not on a Reflex bike's.

Why: the 50cc's tyres are 0.6 ft (Reflex 1.17 / 1.04), its swingarm 1.0 ft (1.78), its fork attachment (0, 2.5, 0.1), torque about half a
125's; it has its own hierarchy (`MX_Hierarchy_50`, 14 nodes, the parent tree of Reflex's 18). The 650 has a 450F's geometry but body mass
9.6 (6.8), springs 1600 / 1500 (1100 / 950), an 8000 rpm limit, more low-end torque, final drive 3.23.

How:
* **Core** (`game/phys_studio.c` "ALIVE CLASSES", `rx_session_vehicle_physics` / `rx_session_vehicle_physics_nodes` in `rx_api.h`): before the
  event loads the client hands the core the bike's parameter file (Alive's bxml as it is: the `mx*_vehicle` schema, root type `mx`) for a
  VehicleModelId. When a LOCAL player's vehicle of that model is made, its class file is read as always (0043a3f0), then the Alive file's
  records (100 of the 108; the other 8 stay the base class's: the 125's for the 50cc, the 450F's for the 650) are written over it, at the
  hook PHYSICS STUDIO uses (`rx_ps_params_loaded`), before the masses are derived. `rx_session_local_phys_class` reports 9 (Alive 50cc) /
  10 (Alive 650). With nodes (the 50cc) the made vehicle's model hierarchy (V+0x1328) takes the bike's own rest locals, so the model pose
  (`rx_session_bike_pose`) has its geometry. Computer riders and the other online riders keep their own. Nothing changes without a call.
* **Importer**: the 50cc rides in the Dakumoto 125's garage slot (ownership, price), the 650 in the Dakumoto 450F's; each `.rxmod` folder
  gets `physics/<file>.bxml` (and the 50cc `physics/nodes.txt`, its hierarchy) and `"physics": {"file", "class", "nodes"}`, `"class"` (the
  garage section "50CC" / "650"), the 50cc `"pose": "physics"` + `"fit": "keep"`.
* **Client**: `BikeMods.OwnPhysics` (the bikes the player has chosen) -> `OpenReflexSimulation` calls the core before the load. A `"pose":
  "physics"` bike is drawn at its own size on its own rest pose; `OriginalBikePose.ForkFix` turns the steering assembly onto the axle the
  physics places (Alive's file has a 125's rake: without it the upper and lower forks parted and the fender floated off the legs); the
  rider's hands and feet go to the mini's own grips and pegs (`RiderLimbRetarget` offsets from its locators). The garage lists the two in
  sections of their own (50CC, 650) with their class on the info panel.

Checks (`core/tools/alive_class_probe.sh`: a full-throttle run on the reference National, 1 rider): Blaze 125S stock 85.3 ft/s top, with
Alive's mx50 61.6 ft/s (12,500 rpm in 5th, its own gearing), class 9; Blaze 450F stock 81.6 ft/s, with mx650f 76.1 ft/s (slower to the top
and steadier at 8000 rpm), class 10. Races (`rxmod_ride.py`, 5 AI): the 50cc PASS (12.4 in fork / 10.7 in rear travel, wheels on the ground,
the fork and fender one piece, hands on the bars), the 650 PASS (15.8 / 14.4 in). Garage pages: both cards, their sections, the stage at
the mini's own size.

Known limits: the rider is Reflex's full-size rider (MX animations) fitted to the mini's grips and pegs, not Alive's 50cc animation set; a
remote 50cc is drawn as the original; both use the engine sound of their base slot.

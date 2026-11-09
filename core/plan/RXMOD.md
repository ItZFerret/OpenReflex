# `.rxmod` bike mods

Status: **experimental** (branch `experimental`, 2026-11-08). Reader: `Unity/Reflex/Assets/Scripts/Content/RxModBikes.cs`; builder:
`Content/ReflexBikeModel.RxMod.cs`; garage / scan: `Content/BikeMods.cs`; `OriginalWorld/ReflexRoster.cs` (`ApplyRxBike`).
Tools: `tools/rxmod/rxmod.py` (check / pack / list / extract) and `tools/rxmod/blender_rxmod.py` (Blender -> `.rxmesh`). Modder guide: `tools/rxmod/README.md`
(a copy sits in the shared `Mods/Bikes` folder as `RXMOD guide.md`).

## What an `.rxmod` is

A **whole new bike** — its own model and its own textures — drawn **over one of the original game's bikes**. The original bike stays the
vehicle: its VehicleModelId, its engine, gearing, suspension, handling, mass, price and ownership are used **1:1**; nothing in an `.rxmod`
can change physics. The core never reads a mod (it simulates the original bike); only Unity draws the mod's model in its place, posed by
the original bike's `MX_Hierarchy` (steering, fork travel, swingarm, wheel spin, levers) exactly as the original model is.

The classic PC bike mods (`.dx9.database` / `.dx9.package` re-skins of the game's packages, `BikeMods.cs`) keep working unchanged next to
it; an `.rxmod` needs none of the game's formats and holds nothing of the original game.

## Layout

An `.rxmod` is a Reflex archive (RXAR, `core/plan/RXARCHIVE.md`: zstd or stored entries, an index, CRC-32 + SHA-256 per entry) whose
header kind is **`MODS`** (`0x53444F4D` as a little-endian u32). While a modder works on it, the same files in a **plain folder** load the
same way (a folder whose `mod.json` says `"format": "reflex-mod"`).

`rxmod.py pack` writes an open archive: anyone can list and extract it. The mod's folder is its source: keep it, and pack it again to
change the mod.

```
mod.json                      the manifest (below)
preview.png                   optional: the mod's thumbnail
logos/yamaha.png              optional: a bike's brand logo on the garage stage
yz250f/bike.rxmesh            a bike's mesh (below)
yz250f/diffuse.png            its maps: PNG or JPG, power-of-two sizes, at most 4096
yz250f/spec.png
yz250f/norm.png
README.txt                    optional, packed along
```

Several bikes may share files (a 250 and a 450 drawn from one model: one `.rxmesh`, two bikes; the archive stores equal bytes once).

### Manifest (`mod.json`)

```json
{
  "format": "reflex-mod", "version": 1, "type": "bikes",
  "name": "2023 Yamaha YZ-F", "author": "NFG COLE", "description": "2023 YZ250F and YZ450F",
  "thumbnail": "preview.png",
  "bikes": [
    {
      "bike": 31, "original": "Takashi 250F",
      "brand": "Yamaha", "model": "YZ250F", "logo": "logos/yamaha.png",
      "mesh": "yz/bike.rxmesh", "fit": "axles",
      "materials": {
        "bike":  { "template": "plastic", "diffuse": "yz/yz250_diffuse.png", "spec": "yz/yz250_spec.png", "mud": 1.0 },
        "tires": { "template": "tire", "diffuse": "yz/yz250_diffuse.png", "spec": "yz/yz250_spec.png", "normal": "yz/yz250_norm.png" }
      }
    }
  ]
}
```

| field | meaning |
|---|---|
| `format`, `version`, `type` | `"reflex-mod"`, `1`, `"bikes"` (or `"skins"`: see "Skins"); a newer version is refused with a message |
| `id` | optional: the mod's stable id, which skins name (`"nfgcole-2023-yamaha-yz-f"`); the name works too |
| `bikes[].id` | optional: the bike's id in the mod, which skins name (`"yz250f"`); the model line or the VehicleModelId work too |
| `name`, `author`, `description` | the garage card's lines: the card says `BY <author>` |
| `thumbnail` | a picture in the mod |
| `bikes[].bike` | **the original bike it rides as**: that bike's VehicleModelId (table below). Required |
| `bikes[].original` | that bike's name, for people reading the file (not read) |
| `bikes[].brand`, `.model` (or `.name`: `"Brand Model"`, `"Brand Words \| Model"`) | the card's brand and model lines |
| `bikes[].logo` | a brand logo picture in the mod |
| `bikes[].mesh` | the bike's `.rxmesh` |
| `bikes[].fit` | `"axles"` (default: scaled so both axles sit exactly on the original's) or `"keep"` (the mod's own size, aligned on the rear axle) |
| `bikes[].materials.<name>` | each material the mesh names: `template` (below), `diffuse`, `spec`, `normal`, optional `sfx` (an authored SFX map instead of the one made from `spec`), `mud` (0..1, default 1), `shine` (a scale of the spec map, default 1), `twoSided`, `alphaTest` + `alphaRef`, `normalFlipY`, `numbers` (true: the rider's run-time number plate on the mesh's second UV set, laid out as the original plastics' "Numbers" plate - the imported Alive bikes' plastics; default false: clear) |

### Mesh (`.rxmesh`)

Little-endian; `str` = u16 byte length + UTF-8.

```
"RXMS"  u32 version = 1  u32 flags = 0
u32 nodes;      each: str name, i32 parent (-1 root, else an earlier node), f32[16] world matrix (row-major; column vectors:
                the translation is elements 3, 7, 11)
u32 materials;  each: str name (a key of the bike's "materials")
u32 surfaces;   each: u32 material, u32 node, u32 flags (1 skinned, 2 uv1, 4 tangents), u32 vertices V, u32 indices I,
                f32[3V] positions, f32[3V] normals, f32[2V] uv0, [f32[2V] uv1], [f32[4V] tangents],
                [u8[4V] bone nodes (255 = none), f32[4V] weights], u32[I] triangle list
```

* Space: **feet**, **X right, Y up, Z forward** (the game's: left-handed), any origin. Triangles are **clockwise** seen from outside
  (Blender's counter-clockwise faces reversed by the exporter when it swaps the axes). UVs as Blender has them (origin bottom-left).
* Nodes are named as the original hierarchy's and sit at the mod's own pivots: `OBJ_Frame` (root), `OBJ_UPR_Forks` (on the steering axis,
  its Y axis along it), `OBJ_LWR_Forks` (the fork legs, same axis), `OBJ_FRT_Tire` / `OBJ_BCK_Tire` (**the axles: required**),
  `OBJ_Swingarm` (the pivot), `OBJ_Throttle`, `OBJ_ClutchLVR`, `OBJ_BrakeLVR`. A node the original has not is drawn on its nearest
  ancestor the original has.
* A rigid surface moves with its node. A skinned one is weighted over up to four nodes per vertex (the rear shock between `OBJ_Frame`
  and `OBJ_Swingarm`, so it compresses as the swingarm swings).

## Skins

A **skin** is a `.rxmod` of its own that repaints a bike of another `.rxmod` — a livery made on the bike's PSD template — without its mesh:

```json
{
  "format": "reflex-mod", "version": 1, "type": "skins",
  "name": "Monster Energy Replica", "author": "Someone", "thumbnail": "preview.png",
  "skins": [
    { "mod": "nfgcole-2023-yamaha-yz-f", "bike": "YZ250F", "name": "Monster Energy",
      "maps": { "yz250f/diffuse.png": "skin/diffuse.png" } },
    { "mod": "2024 Kawasaki KX", "bike": ["KX250", "KX450"], "name": "Monster Energy",
      "materials": { "body": { "diffuse": "skin/kx_body.png", "spec": "skin/kx_spec.png" } } }
  ]
}
```

| field | meaning |
|---|---|
| `skins[].mod` | the bike mod it paints: its `id` or its `name` (case, spaces and punctuation ignored) |
| `skins[].bike` | which bike of it: its `id`, model line (`YZ250F`), brand + model or VehicleModelId; a list paints several (one entry each); empty: the mod's only bike |
| `skins[].name` | the skin's name: the garage card reads `<model> · <name>`, `BY <author>` |
| `skins[].maps` | `{"<the bike mod's map entry>": "<this skin's entry>"}`: every material of the bike that uses that map takes the skin's (the easy way: the bike's diffuse atlas replaced) |
| `skins[].materials` | per material of the bike: `diffuse`, `spec`, `normal`, `sfx`, `template`, `mud`, `shine` replace the bike's; what is not given stays the bike's |
| `skins[].logo` | optional brand logo on the stage (else the bike's) |

* **Loading** (`BikeMods.ResolveSkins`, after the scan): each skin finds its bike among the installed `.rxmod` bikes (`RxModBikes.Paints`). A
  skin whose bike mod is not installed is logged and left out. A skin mod holds at most one skin per original bike (the garage's choice is
  per mod and original bike): a second is logged and left out — put it in a skin mod of its own.
* **Garage**: each skin is a MOD card under the original the bike rides as, next to the bike's own card. Choosing it draws the bike with
  the skin's maps (`BikeSpec.rxSkin`; `RxSkin.Material` merges them over the bike's materials; each map is read from the mod that holds it).
* **Physics**: unchanged — a skin paints a bike that rides as an original 1:1.
* **Making one**: `python3 tools/rxmod/rxmod.py new-skin <bike .rxmod> <folder> --bike YZ250F --name "My livery" --author "Me"` copies the
  bike's diffuse map(s) into `<folder>/skin/` as the starting point and writes the `mod.json`; paint them (the bike's PSD template, exported
  with `psd_compose.py` or Photoshop), `rxmod.py check <folder> --base <bike .rxmod>`, try the folder in `Mods/Bikes`, `rxmod.py pack`.
  A skin is made over the bike author's template: credit them and follow their terms.

### Skins of the original bikes

A skin entry with `"original": <VehicleModelId>` (and no `"mod"`) paints one of the game's own bikes: its maps replace the graphics kit's
textures, whatever kit is equipped. `"maps"` keys are roles — `"plastics"` (the `MX_Plstc_*` materials), `"seat"` (the `MX_Seat_*`
material: it samples the **full-size** bike's plastics atlas, so a 250's seat is the second texture of its kit), `"frame"`, `"engine"`,
`"rims"`, `"tires"` — or a texture's own name (`"MX_Plstc_Venus_Lite_Yellow_THQ"`, `HR_` ignored). The garage card reads `<skin name>`
under the original bike's brand. `ReflexBikeModel.RxOriginalSkin` swaps the diffuse texture of the matching original material (the program,
SFX map, mud and the rider's number plate stay the original's), in the race model and the garage's HR twin alike: paint on the HR template
(2048 x 1024; the race's 1024 x 512 shares its UV layout).

```json
{ "format": "reflex-mod", "version": 1, "type": "skins", "name": "Blue Takashi", "author": "Me",
  "skins": [ { "original": 31, "name": "Blue", "maps": { "plastics": "skin/plastics.png", "seat": "skin/seat.png" } } ] }
```

Templates: `python3 tools/rxmod/original_template.py "<install>/Database" --list` (each chassis and its kits), then
`original_template.py "<install>/Database" VenusF_Lite <out>` writes every kit's plastics / seat textures (normal and HR) as PNG from the
player's own install. The chassis of each bike: the table "The original bikes" below.

### Kit packs (the classic `.dx9` graphics kits and rider gear)

A classic kit pack — `MXTables_DLC0NN` + `MXDLC0NN_graphicskit` / `RiderDLC0NN_graphicskit` / `MXUI_DLC0NN` `.dx9` files, the PC game's
community "DLC" kits (`Content/KitPacks.cs`) — packs into a `.rxmod` of `"type": "kits"` byte for byte (its files at the root, beside `mod.json`):
`python3 tools/rxmod/rxmod.py pack-kit <kit pack folder> <out.rxmod> --name N --author A`. It goes in **`Mods/Gear`**: the archive installer
(`TrackArchives`, kind `RxMod`: an RXAR of kind `MODS` whose metadata says `"kits"`; a `.rxtrack` / `.rxdata` is never unpacked) unpacks it
into a folder beside it on the first scan, like a downloaded zip, and the pack loads as before (GARAGE > VEHICLE > GRAPHICS KIT of its
bikes, rider gear in GARAGE > RIDER). A kit `.rxmod` in `Mods/Bikes` is refused with that hint; a bike / skin `.rxmod` in `Mods/Gear` too.

Imported MX vs ATV Alive bikes are `.rxmod` folders too, made by the in-client importer from the player's own Alive install into
`<content>/AliveBikes/` and scanned next to `Mods/Bikes` (ids `AliveBikes/<slug>`): `core/plan/ALIVE_BIKES.md`.

## How the game loads it

1. **Scan** (`BikeMods.Scan`, once per start and after a rescan): under `<client root>/Mods/Bikes/` (the folder next to the app; the
   desktop players share `REFLEX-lead/builds/player/Mods`) every `*.rxmod` file and every folder with a reflex-mod `mod.json` is a mod
   (id: its path relative to `Mods/Bikes`). `RxModBikes.Open` reads the archive's index and `mod.json`; a broken mod is logged
   (`[RxMod] <path>: <why>`) and left out.
2. **Garage**: `BikeMods.ModsFor(original)` lists the mods with a bike for that VehicleModelId; each shows as a **MOD card** under the
   original (`OpenShell.GaragePages.ModCard`), with the mod's brand / model lines, logo and `BY <author>`. Choosing it stores
   `BikeModel.Model.<VehicleModelId>` = the mod id, as for the classic mods; TEST RIDE, race setup and the race use the choice
   (`BikeMods.ForPlayer`). Computer riders keep the original bikes.
3. **Build** (`ReflexBikeModel.BuildRx`): the original spec's `MX_Hierarchy` gives the pose nodes; the mesh is read (cached) and **fitted**:
   rear axle onto the original's rear axle, the axle line onto the original's (a pitch about X), scaled to the original's wheelbase
   (`fit: axles`); each wheel's parts are then scaled about their axle to **the original tyre's radius**, so the wheels touch the ground
   where the physics rolls them (a correction beyond 15 % is refused and logged). **A mod without wheels** (no surface on `OBJ_FRT_Tire` /
   `OBJ_BCK_Tire`: MX Simulator / MX Bikes models keep their wheels apart) gets **the original bike's own tyres and rims** on its axles
   (`RxOriginalWheels`, drawn as `Build` draws them, with their own materials). The log line
   `Reflex bike <mod> / <bike>: ... scale S, pitch P deg, steering axis D in off the original's, A deg apart, OBJ_FRT_Tire r in -> r' in, ...`
   says how far the mod's own rig is from the original's.
4. **Materials**: each mod material **borrows an original material of the bike it rides as** (`template`), resolved as the game
   resolves that binding (the graphic kit first), so the original pixel program, its constants, mud, lighting and the ENHANCED material
   class all apply. The mod's maps replace the original's:

| template | the original binding | program | the SFX / combo map made from the spec map (R, G, B) |
|---|---|---|---|
| `plastic` | `MX_Plstc` (kit plastics) | Template_Vehicle_Plstc_Fnl | 0 (R lights the diffuse itself on plastics), mud, spec (reflection) |
| `frame` | `MX_Frame_*` | Template_MX_Frame_FNL | spec (shine blend), mud, spec |
| `engine` | `MX_Eng_*` | Template_ATV_MX_Engine_FNL | spec, mud, spec |
| `wheel` | `MX_Rim` | Template_MX_Wheel_Fnl | spec, mud, spec |
| `tire` | `MX_Tire_*` | Template_tire_FNL | spec, mud, spec; the normal map (8-bit -> signed RGHalf) |
| `chain` | `HR_MX_Chain` | Template_ATV_MX_AlphaMtl_UI_FNL | spec, mud, the diffuse's alpha (cut-outs) |

   G is `mud` x 0.5 x 255 (where mud may build up: `mud = saturate(G + usr_mud_amount - 1)`, so a fully muddy bike cakes half the
   surface, like the original plastics' mask). A template is borrowed only when its program is one of these six (`RxPrograms`): the
   kit's material for the binding, else the chassis' own material of that name, else the reference bike's (MarsF Lite, THQ kit). The KTM
   450's kit plastics use another program (Template_ATV_MX_Plstc: a dirt mask and a wet-mud normal map in other slots) and the HR twins
   name their bindings `HR_MX_Plstc`: both found by this rule (before it, the mod bikes on KTM 450 SX-F drew black plastics and every mod
   bike white plastics in the garage). Outside a race nothing sets `usr_mud_amount`: the materials start at 0 (clean) so the garage and
   menus show a clean bike. The original's mud noise stays; its "Numbers" plate texture is left clear (an `.rxmod`
   bike's plates are in its own diffuse); the frame's fork sticker (second UV set) is clear. Maps are uploaded raw (no sRGB conversion,
   like the original's DXT data), mipmapped and block-compressed at load (`RxModBikes.Map`); SFX / normal maps are made at 2048 at most.
5. **Physics**: none of the above reaches the core. The race and the AI simulate the original bike.

## Making one (Blender + PSD -> `.rxmod`), step by step

The full modder guide is `tools/rxmod/README.md` (tools: `blender_rxmod.py` the exporter, `rxmod.py` check / pack / list / extract, `psd_compose.py` a PSD template's diffuse / spec / normal without Photoshop; for skin exporters `RxModSkinWriter.cs`, a C# skin
writer on `tools/rxtrack/RxTrackWriter.cs`, and `Reflex Skin Mods Guide.html`, the onboarding guide of the skin exporter kit; `Reflex Bike Mods Guide.html`, the onboarding guide
of the bike kit `rxmod-bike-kit.zip`, whose worked sample is NFG COLE's FC 250 (his `.blend`, recipe, maps, mod folder and `.rxmod`,
shared with his permission for the kit only: not in git; the zip's guide copy embeds the previews and in-game pictures, the repo copy
doesn't). In short:

1. **Model** in Blender at real scale (metres), the bike standing on its wheels, its nose along an axis (+Y, -Y, +X or -X), Z up. One
   UV atlas (or a few) for the whole bike is fine.
2. **Textures**: export the diffuse from the PSD template with the cover / welcome / UV / palette layers hidden (a power of two, 4096 at
   most; 4096 for one atlas of the whole bike), the spec (or gloss) map, and the normal map (only the tyre program uses it).
3. **Recipe**: a small JSON telling the exporter which objects are the front wheel, the fork legs, the upper forks / bars, the swingarm,
   the rear wheel, the rear shock, the levers (regular expressions on object / material names; position rules for merged meshes) and which
   Blender material is which mod material.
4. **Export**: `blender -b bike.blend --python tools/rxmod/blender_rxmod.py -- --recipe recipe.json --out mymod/bike/bike.rxmesh
   --report report.json --preview side.png`. Open `side.png`: every part coloured by the node that moves it (yellow front wheel, cyan fork
   legs, blue bars / upper forks, magenta swingarm, orange rear wheel, grey frame, red skinned shock).
5. **Manifest**: `mod.json` as above: the original bike (VehicleModelId) each bike rides as, its names, the author, its materials.
6. **Check** `python3 tools/rxmod/rxmod.py check mymod` and try the folder in `Mods/Bikes` (it loads unpacked).
7. **Pack** `python3 tools/rxmod/rxmod.py pack mymod "My Mod.rxmod"` and share the file.

## The original bikes (VehicleModelId)

The motocross bikes a mod can ride as (from the garage's catalog; the brand is the original's fictional one):

| VehicleModelId | original bike | class |
|---:|---|---|
| 11 | Blaze 250F (Honda) | MX Lite (250F) |
| 1 | Blaze 450F (Honda) | MX (450F) |
| 13 | Dakumoto 250F (Kawasaki) | MX Lite (250F) |
| 14 | Dakumoto 450F (Kawasaki) | MX (450F) |
| 18 | KTM 250 SX-F | MX Lite (250F) |
| 28 | KTM 450 SX-F | MX (450F) |
| 17 | Shima 250F (Suzuki) | MX Lite (250F) |
| 10 | Shima 450F (Suzuki) | MX (450F) |
| 31 | Takashi 250F (Yamaha) | MX Lite (250F) |
| 32 | Takashi 450F (Yamaha) | MX (450F) |
| 12 / 35 / 36 / 6 / 29 | Blaze 125S / Dakumoto 125 / KTM 125 SX / Shima 125R / Takashi 125R | 125 two-strokes |
| 27 / 16 / 15 / 9 / 30 | Blaze 250R / Dakumoto 250S / KTM 250 SX / Shima 250R / Takashi 250R | 250 two-strokes |

## The NFG COLE pack (2026-11-08)

Source: `Open Reflex .psd & .blend Pack by NFG COLE.zip` (33 files: Blender models and PSD / PNG skin templates), author **NFG COLE**
(credited in every mod's `author` and on its garage cards). Converted into six `.rxmod` files in the shared `Mods/Bikes/NFG COLE/`
(not in git: the author's art). Each 250 rides as the original 250F class bike, each 450 as the original 450F class bike, of the same
brand where the original game has it:

| mod bike | source model / skin | rides as (physics 1:1) |
|---|---|---|
| 2025 Honda CRF250R | `2025 CRF 450r.blend` (the 450's model), 450 skin | Blaze 250F (#11) |
| 2025 Honda CRF450R | `2025 CRF 450r.blend`, `2025 CRF450.psd` | Blaze 450F (#1) |
| 2023 Yamaha YZ250F | `2023 YZ250f.blend`, `2023 YZ250F.psd` | Takashi 250F (#31) |
| 2023 Yamaha YZ450F | `2023 YZ450f.blend`, `2023 YZ450F.psd` | Takashi 450F (#32) |
| 2024 Kawasaki KX250 | `2024 KXF 450.blend` (+ `dependent.blend`, one shared model), diff / norm / spec PSDs | Dakumoto 250F (#13) |
| 2024 Kawasaki KX450 | the same | Dakumoto 450F (#14) |
| 2024 GASGAS MC 250F | `2024 MC450.blend` (the 450's model), `2024 MC450.psd` | KTM 250 SX-F (#18): GASGAS is KTM's platform |
| 2024 GASGAS MC 450F | `2024 MC450.blend`, `2024 MC450.psd` | KTM 450 SX-F (#28) |
| 2025 Husqvarna FC 250 | `2025 FC 250 RE.blend`, `2025 FC 250 RE.psd` + PNG set | KTM 250 SX-F (#18): Husqvarna is KTM's platform |
| 2025 Husqvarna FC 450 | the FC 250's model and skin | KTM 450 SX-F (#28) |
| 2023 TM Racing 250Fi | `TMRacing_2023_250.blend` (its maps packed in it), `2023 TM250cc Template 8K.psd` | Shima 250F (#17): the original has no TM; Shima (Suzuki) is the brand no other bike of the pack takes |
| 2023 TM Racing 450Fi | `TMRacing_2023_450.blend`, `2023 TM450cc Template 8K.psd` | Shima 450F (#10) |

### Pack status (2026-11-08, experimental 466458c8 + this doc)

Conversion: the `.blend` files opened in Blender 5.2 headless (`-Y`, auto-run scripts off, under a `sandbox-exec` profile with no network
and writes only to the output folder); `tools/rxmod/blender_rxmod.py` with one recipe per model; the PSD skins rebuilt with
`tools/rxmod/psd_compose.py` without their "Welcome" / palette cover layers (checked against Photoshop's own composite: mean 0.24 / 255),
the TM maps taken from the PNGs packed inside the TM `.blend` files. Diffuse 4096, spec 2048. Installed: the shared
`REFLEX-lead/builds/player/Mods/Bikes/NFG COLE/*.rxmod` (6 files, 17-34 MB each) and `Mods/Bikes/RXMOD guide.md`.

Tests (every screenshot opened full size): `tests/menus_showcase` page `rxmod-garage` (12 MOD cards "BY NFG COLE", A equips the mod,
three stage angles each, race setup's 3D hero = the mod bike), and `tests/rxmod/rxmod_ride.py` per bike (batch, muted, no focus): a
1-lap National at NAT_Concord with 5 AI on a pro's bike of the original model, close-ups at the gate and riding, air shots, the fork /
swingarm travel measured on the mod's parts (all 14-17 in), finish; and the online fallback in-game (`ReflexRoster.ForLook` with the
bike's token draws the mod, an unknown token the original). Loopback online suite: 2 mod riders + 1 without hold each other's tokens.

| bike | rides as | garage | race (finished) | notes |
|---|---|---|---|---|
| Honda CRF250R / CRF450R | Blaze 250F / 450F | ok | ok / ok | one model and skin (the 450's) for both |
| Yamaha YZ250F / YZ450F | Takashi 250F / 450F | ok | ok / ok | own model and skin each |
| Kawasaki KX250 / KX450 | Dakumoto 250F / 450F | ok | ok / ok | one shared model and skin; Pro Circuit pipe + Renthal bar variant |
| GASGAS MC 250F / MC 450F | KTM 250 SX-F / 450 SX-F | ok | ok / ok | the MC 450's model and skin for both; Akrapovic + FatBar variant |
| Husqvarna FC 250 / FC 450 | KTM 250 SX-F / 450 SX-F | ok | ok / ok | the FC 250's model and skin for both; FMF + ProTaper variant; plain template plates (white) |
| TM Racing 250Fi / 450Fi | Shima 250F / 450F | ok | ok / ok | own wheels (scaled ~4 % to the original tyre); the 450 wears the 250's skin ("250 FI" decal): the pack's 450 PSD is empty (no layers, a blank composite) |

Known limits: the steering axes sit 1.5-2.8 in from the originals' (the forks turn about the original's axis: a slight swing at full
lock); the bikes without wheels of their own (all but TM) ride on the original bike's tyres and rims; the original's 2-rider
in-game online session (two players, one host) was not run here, only the protocol (loopback) and the in-game resolution of a look.

### Skins status (2026-11-08)

Tested with test-only skins (in `builds/nfgcole/skinroot`, not installed): a YZ250F skin by `"maps"` (the atlas, red / blue swapped), a
KX450 skin by `"materials"` (only `body`, inverted: the frame / forks keep the bike's maps), and an original Takashi 250F (#31) skin
(`"original": 31`, plastics + seat from `original_template.py`'s HR templates, swapped). Garage page `rxmod-garage` (REFLEX_RXMOD_ONLY=Test):
cards `YZ250F · TEST RED`, `KX450 · TEST BLUE BODY`, `TEST BLUE` (Takashi), A equips the skin, race setup's hero shows it; races with AI
(`tests/rxmod/rxmod_ride.py`): the YZ skin and the original skin both finish, the original's run keeps the rider's number on its plates;
online in-game: skin token -> painted, unknown skin token -> the bike's own maps / the original. Loopback: a skin with its bike, a skin alone,
malformed tokens refused. Kit packs: the six installed bike kits (`Mods/Gear`: Red Bull KTM (Dakumoto), BARX Suzuki, Chase Sexton HRC,
Jett Lawrence, Jorge Prado Kawasaki, Ken Roczen SX Paris) packed with `pack-kit` into a test `Mods/Gear` of `.rxmod` files only: all six
unpack and load (showcase `gearmods`: ALL PASS, every kit previewed on its bikes, a race).


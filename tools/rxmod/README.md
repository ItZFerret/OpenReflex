# Making an Open Reflex bike mod (`.rxmod`)

An `.rxmod` puts **your own bike** — your model, your textures — into Open Reflex. It rides **as one of the game's bikes**: that bike's
engine, gearing, suspension, handling, price and ownership are used exactly (1:1). Your mod only changes how the bike looks. Players drop
the `.rxmod` file into `Mods/Bikes/` (next to the game) and find your bike in **GARAGE > VEHICLE** as a **MOD** card under the bike it rides
as, with your name on it.

The full format reference is `core/plan/RXMOD.md`. This page is the how-to.

## What you need

* **Blender** (4.x or 5.x) with your bike model.
* Your **textures**: a diffuse (colour) map — usually your PSD template exported with the "welcome" / UV / palette layers hidden — and a
  spec (or gloss) map. PNG or JPG, a power of two in size, **4096 at most** (4096 for one atlas of the whole bike, 2048 for a spec map).
* **Python 3** (any; no packages) for `tools/rxmod/rxmod.py`.
* No Photoshop? `tools/rxmod/psd_compose.py` rebuilds a template's diffuse without its cover layers
  (`compose template.psd diffuse.png --skip Welcome --skip "Color Palet"`) or one of its hidden groups (`--only Spec --flatten 0,0,0`).
  It needs numpy: run it with Blender's own Python.

## 1. Pick the original bike your bike rides as

Your bike keeps that bike's physics. Use the same class (a 250F rides as a 250F, a 450 as a 450) and the same brand when the game has it:

| VehicleModelId | original | class |
|---:|---|---|
| 11 / 1 | Blaze 250F / Blaze 450F (Honda) | 250F / 450F |
| 13 / 14 | Dakumoto 250F / 450F (Kawasaki) | 250F / 450F |
| 18 / 28 | KTM 250 SX-F / 450 SX-F | 250F / 450F |
| 17 / 10 | Shima 250F / 450F (Suzuki) | 250F / 450F |
| 31 / 32 | Takashi 250F / 450F (Yamaha) | 250F / 450F |
| 12, 35, 36, 6, 29 | the 125 two-strokes (Blaze 125S, Dakumoto 125, KTM 125 SX, Shima 125R, Takashi 125R) | 125 |
| 27, 16, 15, 9, 30 | the 250 two-strokes (Blaze 250R, Dakumoto 250S, KTM 250 SX, Shima 250R, Takashi 250R) | 250 2T |

Several mods may ride as the same bike: each gets its own card.

## 2. Set the model up in Blender

* **Real size.** Any unit is fine (metres, or feet as MX Simulator / MX Bikes models use): the game fits your bike to the original's axles.
* The bike **stands on its wheels, Z up**, its nose along +Y, -Y, +X or -X.
* **Split it into the parts that move**, as separate objects (or separate materials), like an MX Bikes model:

| part | what's in it | moves with |
|---|---|---|
| frame / body | frame, engine, plastics, seat, tank, exhaust, footpegs, rear fender | the bike |
| upper forks / bars | triple clamps, upper fork tubes, bars, levers, grips, number plate, front fender | the steering |
| lower forks | the fork legs, the front caliper, the disc guard | the steering and the fork travel |
| swingarm | swingarm, chain, chain guide, rear caliper | the rear suspension |
| front wheel / rear wheel | tyre, rim, spokes, hub, disc, sprocket (optional, see below) | the wheel's spin |
| rear shock (optional) | the shock and spring | stretched between the frame and the swingarm |

* **No wheels?** Leave them out: the game then draws the original bike's own wheels and tyres on your axles. You only tell the exporter
  where the axles are (see `axles` below; an axle bolt modelled across the swingarm / the fork legs is easy to find: its centre is the axle).
* Everything visible and set to render is exported; hide the reference images, spare parts and helper objects (or list what to include).

## 3. Write a recipe

A small JSON file telling the exporter which object is which part. Patterns are regular expressions (case-insensitive) on the object name
(`obj:`) or the material name (`mat:`); the first rule that matches wins.

```json
{
  "unit": 0.3048,
  "include": ["^FMF FG$", "^ProTaper$", "^LowerFork$", "^Swingarm$"],
  "offsets": {"FMF FG": [-9, 0, 0]},
  "axles": {"front": [1.9118, 2.6211, -0.8947], "rear": [1.9117, -2.2629, -0.9409]},
  "nodes": [["obj:^ProTaper$", "OBJ_UPR_Forks"], ["obj:^LowerFork$", "OBJ_LWR_Forks"], ["obj:^Swingarm$", "OBJ_Swingarm"], ["obj:.*", "OBJ_Frame"]],
  "skin": [{"match": "mat:^Shock$", "top": "OBJ_Frame", "bottom": "OBJ_Swingarm"}],
  "materials": [["mat:^(Plastic|Seat|GasCap|BarPad|Rubber)$", "body"], ["mat:^Cast", "cast"], [".*", "metal"]]
}
```

* `unit`: metres per Blender unit (0.3048 for a model in feet, 1.0 in metres).
* `offsets`: moves an object into place (a variant laid out beside the assembled bike in the file).
* `axles`: the axle centres in Blender coordinates, when the bike has no wheels; with wheels, name them `OBJ_FRT_Tire` / `OBJ_BCK_Tire`
  and the exporter takes their centres.
* `nodes`: the parts — `OBJ_Frame`, `OBJ_UPR_Forks`, `OBJ_LWR_Forks`, `OBJ_FRT_Tire`, `OBJ_Swingarm`, `OBJ_BCK_Tire`, and optionally
  `OBJ_Throttle`, `OBJ_ClutchLVR`, `OBJ_BrakeLVR`.
* `skin`: parts stretched between two nodes (the rear shock).
* `materials`: which of your Blender materials become which mod material (step 5). Several Blender materials may share one.

## 4. Export

```sh
blender -b bike.blend --python tools/rxmod/blender_rxmod.py -- --recipe recipe.json --out mymod/bike/bike.rxmesh \
        --report report.json --preview side.png
```

Open `side.png`: every part is coloured by what moves it — **grey** frame, **blue** upper forks / bars, **cyan** fork legs, **yellow** front
wheel, **magenta** swingarm, **orange** rear wheel, **red** skinned (the shock). Add `--texture mymod/bike/diffuse.png --view three4`
to see it textured instead (checks your UVs and that you picked the right bars / exhaust variant). `report.json` gives the wheelbase,
the axles, the steering axis and the triangle counts. Around 100 000 triangles for the whole bike is fine.

## 5. Write `mod.json`

```json
{
  "format": "reflex-mod", "version": 1, "type": "bikes",
  "name": "2025 Husqvarna FC", "author": "Your Name", "description": "FC 250 and FC 450",
  "thumbnail": "preview.png",
  "bikes": [
    { "bike": 18, "original": "KTM 250 SX-F", "brand": "Husqvarna", "model": "FC 250", "mesh": "fc/bike.rxmesh",
      "materials": {
        "body":  { "template": "plastic", "diffuse": "fc/diffuse.png", "spec": "fc/spec.png" },
        "metal": { "template": "frame",   "diffuse": "fc/diffuse.png", "spec": "fc/spec.png" },
        "cast":  { "template": "engine",  "diffuse": "fc/diffuse.png", "spec": "fc/spec.png" } } },
    { "bike": 28, "original": "KTM 450 SX-F", "brand": "Husqvarna", "model": "FC 450", "mesh": "fc/bike.rxmesh", "materials": { "...": "..." } }
  ]
}
```

Each material **borrows the look of one of the original bike's materials** — its lighting, reflections and **mud** — with your maps:

| template | looks like | use it for |
|---|---|---|
| `plastic` | the graphic kit's plastics: glossy, reflective | plastics, seat, number plates, tank, grips |
| `frame` | the frame: painted metal, shine from the spec map | frame, swingarm, forks, bars, hardware, exhaust |
| `engine` | the engine | engine cases, castings |
| `wheel` | the rims | rims, hubs, spokes |
| `tire` | the tyres (uses `normal`) | tyres |
| `chain` | the chain: cut-outs from the diffuse's alpha | anything with alpha cut-outs (set `"alphaTest": true`) |

Options per material: `mud` (0..1, default 1: how much mud it takes), `shine` (scales the spec map), `twoSided`, `alphaTest` + `alphaRef`,
`normal` + `normalFlipY` (tyres), `sfx` (an SFX map you made yourself: R shine, G mud, B reflection).

Your number plates and decals are part of your diffuse (the rider's number is not painted on a `.rxmod` bike).

## 6. Check, try, pack

```sh
python3 tools/rxmod/rxmod.py check mymod
```

Then copy the **folder** `mymod` into `Mods/Bikes/` and start the game: it loads unpacked, so you can change and retry. The game's log
(`Player.log`) says how it fitted your bike:

```
Reflex bike 2025 Husqvarna FC / Husqvarna FC 250: 9 surfaces, 81228 vertices, 102398 triangles on MarsF_Lite's hierarchy:
  scale 0.9810, pitch -0.01 deg, steering axis 1.20 in off the original's, 2.3 deg apart, the original's wheels
```

A scale far from 1, or a steering axis many inches off, means your units or axles are wrong. When it looks right:

```sh
python3 tools/rxmod/rxmod.py pack mymod "My Bike.rxmod"
```

and share `My Bike.rxmod`. Players put it in `Mods/Bikes/` (any sub-folder).

Keep your mod folder: it is your source. Anyone can open a `.rxmod` with `rxmod.py extract`, so put your terms in `README.txt` too.

## Skins (a new livery for a bike that's already a mod)

A skin repaints a `.rxmod` bike without touching its model: it is a small `.rxmod` of its own with just your textures.

```sh
python3 tools/rxmod/rxmod.py new-skin "2023 Yamaha YZ-F.rxmod" myskin --bike YZ250F --name "My livery" --author "Your Name"
```

This copies the bike's colour atlas to `myskin/skin/` and writes `myskin/mod.json`. Paint the atlas (best: on the bike's PSD template,
then export the diffuse at the same size), then:

```sh
python3 tools/rxmod/rxmod.py check myskin --base "2023 Yamaha YZ-F.rxmod"
python3 tools/rxmod/rxmod.py pack myskin "My livery.rxmod"
```

Players need the bike's `.rxmod` too: your skin shows as its own MOD card (`YZ250F · My livery`, BY you) under the bike it paints. To
replace only some parts, or the spec / normal maps, list them per material instead (`"materials": {"body": {"diffuse": "...", "spec": "..."}}`,
the material names are the bike's `mod.json` keys). One file can hold skins for several bikes (`"bike": ["YZ250F", "YZ450F"]`), one per
bike. Credit the bike's author: your skin is painted on their template.

## Skins for the game's own bikes

Same idea, for an original bike (Blaze, Dakumoto, KTM, Shima, Takashi ...): your picture replaces its graphics kit.

```sh
python3 tools/rxmod/original_template.py "<your game library>/game files/Archive/Database" --list
python3 tools/rxmod/original_template.py "<your game library>/game files/Archive/Database" VenusF_Lite templates
```

paint over `HR_..._Plstc_...` (2048 x 1024; for a 250 the second plastics file of a kit is its seat), then a `mod.json`:

```json
{ "format": "reflex-mod", "version": 1, "type": "skins", "name": "My Takashi", "author": "Your Name",
  "skins": [ { "original": 31, "name": "My livery", "maps": { "plastics": "skin/plastics.png", "seat": "skin/seat.png" } } ] }
```

`"original"` is the bike's VehicleModelId (the table at the top). Check, try the folder in `Mods/Bikes`, pack, share.

## Classic kit packs as one `.rxmod`

A downloaded kit pack (the `MXTables_DLC0NN ... .dx9` files) can be shared as one `.rxmod`:

```sh
python3 tools/rxmod/rxmod.py pack-kit "Mods/Gear/BARXSuzuki" "BARX Suzuki.rxmod" --name "BARX Suzuki" --author "Kit author"
```

It goes in `Mods/Gear` (not `Mods/Bikes`); the game unpacks it there like a zip and the kits show in GARAGE > VEHICLE > GRAPHICS KIT.

## Credits

Put your name in `author`: the garage shows `BY <author>` on every card of your mod. Ship only your own art.

# PHYSICS STUDIO (w-physstudio, 2026-10-29)

Open Unleashed's Physics Studio, made for Reflex's physics. A port addition, not original code.

## What can be changed

- **Every registered vehicle parameter** of the nine parameter files (EngineDependencies' bxml assets):

  | class | file | parameters | values |
  |---|---|---|---|
  | mx125 / mx250 / mx250f / mx450f | mx125_vehicle .. mx450f_vehicle | 108 each (004392f0) | 401 / 405 / 403 / 405 |
  | atv | atv_vehicle | 103 (00489cb0) | 345 |
  | utv | utv_vehicle | 98 (004c9d40) | 323 |
  | buggy / truck2wd / truck2wdlite | buggy_vehicle, truck2WD[lite]_vehicle | 98 each (004af510) | 307 each |

  The counts differ by the curves' knot counts in each file. 3203 values in all. A value is one component: a float, x / y / z of a vector, the three surface levels of a kind-3
  parameter (the tyre values the game blends by the surface value 0 / 0.5 / 1 under the tyre), or a curve's knot y / x and its end slopes.
- **Groups** (by the files' own prefixes): ENGINE & POWER, GEARING, BRAKES, CHASSIS & MASS (gravity, drag curves, masses,
  inertias), STEERING & LEAN, SUSPENSION, TYRES & GRIP, AIR CONTROL, RIDER.
- **The terrain tables**: the six grip materials of 00461680 (0.1, 0.3, 0.5 default, 0.65, 0.8, 0.9) and the four drag factors of
  00461760 (0, 0.75, 1, 0.25).
- **Defaults** are the files' values, read with the format library the way 0043a560 / 0048b680 read them. The core checks them in a race:
  every component of all nine classes equals the simulation's parameter block bit for bit.
- **Bounds** (w-physdesc adds natural limits per name, see below): the files define none. 0 .. 5x a positive value (5 % .. 5x for masses, inertias, radii, lengths, time constants); 5x .. 0 a
  negative one; +-max(1, the record's largest value) for a 0; switches 0..1; a curve's x between its neighbours. The core clamps to them.
- **Units** (w-physdesc): read off the original's code, per parameter name (`core/tools/phys_studio_desc.txt`): the simulation works in ft, slugs
  (1 slug = 32.2 lb), s and lb-force; curves' engine speeds are rad/s, the engine limits rpm. 11 names have no determinable unit and say "(unknown)".

## Where it applies (core: game/phys_studio.c, hooks marked PHYSICS STUDIO)

- **Load point**: right after each parameter file's apply (0043a3f0, 0048b5d0, 004cb360, 004b0b30, 0044f690), into P = S + 0x2f10.
  The masses (0043b750 / 0048c750) and every derived value are then made from the config's values, as from the file's.
- **The garage tuning still applies on top**, unchanged: 00543130 -> 0045f270 writes the tuning factors S + 0x2ef0.. and vf35
  (0043be80) multiplies the base values by them. The studio sets the base; the sliders scale it.
- **Per player**: only vehicles whose spec has a controller (00543020 >= 0, set in VehicleFactory_CreateVehicle 00542b90). The computer
  riders keep the original's values unless the config says `all_vehicles` (VALUES FOR: EVERY RIDER).
- **The grip table** is per local vehicle (00461680 gets the vehicle) unless `grip_global`. **The drag table** is global: 00461760 has no
  vehicle.
- **Live**: `rx_session_phys_arm_live` before the load keeps the vehicles' file values; `rx_session_phys_set` +
  `rx_session_phys_apply_live` in the event rewrites the block and reruns the masses + vf35, as the original's own developer reloads do
  (0043b690's pending file / 0048d320 / the four-wheel pending reload). Every class (round 2).
- **Records**: a config with changes keeps no offline records, best laps or highlights for the event (as MAP SCALE).
- **The guard**: a config can make the original's solver diverge (a tiny mass on a stiff spring: 800 -> 24,000 -> 680,000 ft/s in three
  frames; far outside the world the terrain grid's lookups then walk garbage and a frame can hang). While a config is set, before each
  frame, each vehicle it reached is checked (bodies finite, under 2000 ft/s, within 60,000 ft). One that fails gets the file's values back
  (the live reload), its bodies the last sane frame's positions with no velocity, and the config is off for it until the next event
  (stderr says so; `rx_session_phys_active` returns 3).
- **No config**: every hook is one test of a static that is 0. Nothing in the guest changes.
- **FREER AIR** (OPTIONS > OPEN REFLEX AUTO LEVEL / COUNTER-STEER SNAP sliders, free_air.c; 100 % = the original's helper, 0 % = removed) composes: it lowers or removes the original's trick hold / auto-level slerps; the studio's AIR
  CONTROL values (stick RPY rates, velocity curves, AngularRateDamping S+5680, the springs) still drive the stick and the coasting spin.
  Lower damping with FREER AIR = the bike keeps turning longer after release. AIR LOOSENESS (w-freeair2) eases the damping
  (S+5680), the take-off damping (S+56e0), the stick filters and the stick's authority relative to the value the simulation holds, so it
  eases the studio's values, not the file's: the studio sets the base, the looseness scales it, the garage tuning is untouched by both.
  All three apply to the local player only.

## The client

- **Entry**: the hub's PHYSICS STUDIO chip (beside GAME LIBRARY). It reads PHYSICS: <NAME> while a config is in use.
- **Pages** (OpenShell.PhysicsStudio.cs): CONFIGS (name / author / notes, SAVE, SAVE AS A COPY, NEW, COPY SHARE CODE, EXPORT .JSON
  FILE, DELETE, ORIGINAL PHYSICS, the library with IN USE / PRESET / OTHER VERSION, PASTE SHARE CODE, IMPORT .JSON FILES, OPEN CONFIG
  FOLDER); VEHICLE (class, group, search, a bar + number per value, the changed ones marked in gold with a dot, RESET GROUP / VEHICLE /
  EVERYTHING, VALUES FOR); TERRAIN (grip, drag, GRIP TABLE FOR). Pad: up / down, left / right adjust (stopping at the file's value),
  A / Enter types a value, Y / F resets the value, LB / RB pages, B / Esc back (twice with unsaved changes). USE FOR MY RIDES / DONE.
- **Configs**: `<ReflexSave.Folder>/Physics/<name>.json` (format `open-reflex-physics` 1: name, author, description, game version,
  paramSet = the catalog hash, gripGlobal, allVehicles, values [vehicle, param, comp, value], grip, drag). Export to Physics/Export,
  import from Physics/Import. Share code: `ORPHYS1:` + base64url(deflate(the compact JSON)), copied to / pasted from the clipboard (found
  anywhere in a pasted chat line). Imports are checked: format and version, depth, size, every name against the catalog, values clamped,
  NaN / text refused; another paramSet or unknown names show OTHER VERSION.
- **Presets** (built from the catalog, read-only, SAVE makes a copy): Original (no values), More Grip (side-slip mu and its dynamic
  multiplier x1.25, rolling mu x1.1, every class), Lighter Air (gravity x0.8), Snappy Power (torque curve x1.2).
- **Where**: exhibition races, Free Ride and the garage's test ride; the career, MotoSkills and the challenges with PHYSICS STUDIO EVERYWHERE (practice). Never online.
- **Badge**: PHYSICS MODIFIED · <NAME> at the top of the race screen (PhysicsStudioBadge) and a PHYSICS MODIFIED tile in the pause page.

## Round 2 (w-physstudio2)

- **The gate** (`Reflex.Port.RxCore.RxPhysicsStudio`, Unity/Reflex/Assets/Scripts/OpenReflex/RxPhysicsNative.cs) decides alone whether a
  session gets a config. `SetOnlineSession(true)` before an online session is created (false when it ends): nothing of the player's
  library applies while online, live edits included; `SetOnlineHostConfig(text)` (only while online) is for a host protocol that chooses
  one config for everyone. `AllowedHere(PhysicsContext)`: offline races, Free Ride and the test ride always; the career, MotoSkills and
  the challenges only with `Everywhere` (OPTIONS > OPEN REFLEX > PHYSICS STUDIO EVERYWHERE, off by default). `TextFor(context)` is what
  OpenReflexSimulation sends; `LiveEditAllowed(context)` (offline races and Free Ride) arms the live path.
- **Career progress**: a career / MotoSkills / challenge session that rides a config is **practice**. The profile is imported as usual,
  but `PhysicsPracticePersistence` publishes nothing it checkpoints or completes (no progress, rewards, records); the flow goes on.
- **Records**: the core marks the session once a config with changes was set (`rx_session_phys_active` bit 4), a live edit included;
  MXRecords_Leave 00764560 then commits no working row and the highlights take no progress.
- **Live editor** (PhysicsStudioOverlay.cs): PAUSE > PHYSICS STUDIO (the pad and keys edit, the race stays paused; changes reach the
  bike when it rides on) or ` (backquote; ignored when the game's controls bind it): pinned at the right while riding, the mouse edits.
  The hub page's vehicle / group / search / bar rows, compact. SAVE TO CONFIG, SAVE AS, USE FOR MY RIDES, REVERT TO THE CONFIG, REVERT TO
  ORIGINAL, RIDE WITH THE PANEL / EDIT WITH THE PAD. Each change: rx_session_phys_set + rx_session_phys_apply_live at the next frame.
- **Live for every class**: MX, ATV, UTV, buggy and trucks rerun their own masses (0043b750 / 0048c750 / 004cc030 / 004b1800 / 00450480,
  each + vf35), the original's reload sequence. With EVERY RIDER the computer riders change live too; a vehicle that held config values
  gets the file's back when it no longer does. Values the original computes only when the vehicle is built (collision shapes, the wheel
  bodies' geometry) take a change from the next event.

## Proofs (core/plan/QUEUE.md w-physstudio has the numbers)

- No config / an empty config / armed: whole-arena hashes equal main every 600 frames (5 events).
- The catalog equals every class's loaded parameter block bit for bit (phys_studio_test verify).
- More grip: the rear at the lateral limit drops (bike_parity_probe turn ride, phys_studio_rear.py).
- Extreme configs (every value at min / max / random) under the fuzzed pad: no stop.
- Config JSON / share code / file round trips, hostile inputs refused (the C# test in the QUEUE section).

## Labels, descriptions, units, natural bounds (w-physdesc)

- **One table**: `core/tools/phys_studio_desc.txt` (name | label | unit | x unit | bounds | description; one line per distinct name, 156 in all, the
  classes share them). `core/tools/gen_phys_studio_desc.py` checks that every registered name has exactly one line and writes
  `core/game/phys_studio_desc.inc`. The core uses it for the units and the natural bounds (`set_bounds`, `unit_of` in phys_studio.c) and sends it in
  the catalog as `N` lines (name, label, switch flag, unit, x unit, description) before the classes. A core without `N` lines still works: the client
  falls back to the spaced file name. The catalog hash, the config JSON and the share code are unchanged (they carry raw names and component numbers).
- **Client**: the hub page and the live overlay show LABEL (large), the raw name (small), the number with its unit; the side panel / help line says what the
  value does, its unit, the file value and the range. Search matches the label, description, raw name, group and unit (every word must match).
- **Natural bounds** (added to the generic ones; the file's value always stays inside): a switch 0 / 1 (manual transmission, steps by 1);
  sliding-grip shares, air spin keep, wheel spin keep 0 .. 1; steering lock, lean angle, fork rake 0 .. 90 degrees; must stay positive (5 % of the file's
  value at least): spring rates, the gear ratios and reductions, clutch re-engage time; rev limit at least 40 % of the file's, idle at most 2x.
- **Honest limits**: units are the code's (e.g. the masses are slugs: the weights come out right with gravity in ft/s2); the three-value tyre entries are
  indexed by the ground-table value under the tyre (0 / 0.5 / 1), what each ground gives is on the TERRAIN page. The port's names for the tyre's rolling and
  lateral axes cross the original's parameter names in places (the code reads "StaticMuRoll" for the lateral cap and "StaticMuSideSlip" for the friction
  circle): the labels follow the parameter names, which is what the share codes use. 11 names are registered but read by nothing the port found (the bikes'
  air-control springs and stick vectors, the four-wheel arm masses, the clutch coefficient): their lines say "no known effect".

## Not yet

- The original has no ranges; the natural bounds are ours.
- Online: when the online framework comes, the server should choose the config (the share code and paramSet hash are the handle).

# Start grid rows: riders past the gate line up behind it

Branch `w-secondrow` (2026-11-03). PORT ADDITION: `core/game/grid_rows.c` / `grid_rows.h`, called from the grid pass
(`build_activity_grid.c` 005663b0). Online: `OnlineProtocol.GatePlace / GateLabel / MaxGridPlaces`, `OnlineHost.GateSlots`, the gate
pick's labels (`OpenShell.Online`). Tool: `core/tools/grid_rows_test.c`. Alive specifics: `ALIVE_RACING.md` "8. Second row".

## What it does

When a race has more riders than its start gate has `GateStart_NN` slots, the riders past the slot count line up in rows behind the gate,
facing the same way, held by the race's start like the gate's own riders (they creep the same ~1 ft before GO; nothing holds a slot
specially in either game), and ride through the gate when it drops.

- **Grid zones** (the Player List's StartZone = the index of the activity's start slot A + 0x340 + 0x30 z): below the gate's slot count `n` a
  zone is a gate slot (the original); from `n` up a place behind the gate. Row 2 has `n - 1` places (behind the gaps between neighbouring
  slots), row 3 `n` (behind the slots), alternating; a one-slot gate has one place a row. Within a row the zones take the lanes from the
  centre of the gate out; a lane without a clear spot (below) comes last in its row. `rx_grid_place(n, zone)` gives the row;
  `OnlineProtocol.GatePlace` is the same numbering in C#.
- **Capacity.** The places live in the activity's 28 start slots (+0x340 .. +0x880), so a grid holds at most 28 riders: a 16-slot Reflex gate
  takes 12 behind it, an Alive 12-slot gate 16. Past 28 a host-side slot table is needed (the driver's slot pointer d+0x144 is all the race
  reads); see "Past the cap" below.
- **Where.** Every place depends only on the gate, the level and the event's vehicle (never on which places are taken: each rider's game of
  an online race holds a different part of the field and must put a zone in the same spot):
  - axes: forward = the slots' mean direction (flat), across = GateStart_01 -> the last slot; pitch = the median spacing of neighbouring slots
    (an Alive gate's middle gap is a double one); the vehicle = the longest / widest of the riders' classes (the class tables 0x0093582c /
    0x0093580c the game spaces start lines with: a bike 7.5 x 2.5 ft, an ATV 6 x 4.8);
  - preferred depth = the rear of the bikes ahead (row 2: the gate's slots; row 3: the row-2 places within 3/4 pitch) + a vehicle length + a gap
    of half a pitch (1 .. 3 ft): 10.2 ft behind an Alive gate at 110 %, 10.0 behind Concord's;
  - the place is searched along its lane, back from the preferred depth (up to two vehicle lengths), then forward (down to half a foot of
    overlap with the bikes ahead: it stands between them), then on lanes moved across inside a wide gap (as far as keeps a vehicle's width from
    both slots), for the first depth where: the ground under the bike is even (a profile every 0.25 ft on three lines a bike's width apart, the
    terrain or a static surface from a vertical ray: no step over 0.2 ft); the lane to the gate is rideable (no rail or post: 0.5 ft over the
    ground 2.5 ft before and after it; no rise over 1.5 ft; a rise only with 2 ft of run-up); nothing meets the rider's body from the bike's
    rear through the gate (flat rays at 1.5, 2.5 and 4.5 ft, cast from behind: some collision is one-sided);
  - height: the ground there + the gate slots' own height over their ground; the gate slots' direction; the track section / node of the gate
    slot ahead (the place's own track search is kept when it finds the same section; behind some Alive pads it finds no quad); the slot's
    "placed" flag +0x2c = 1 (no start-line offset in RaceDriver::vf3).
- **When** (the original is untouched while the riders fit):
  - a 16-slot gate: nothing is read or written unless the Player List has more rows than 16 or an online grid names a zone >= 16 (never with
    today's core: see below);
  - a gate of 1 .. 15 slots (Alive's 12; a custom one: before, any gate but 12 or 16 slots stopped the core at act_unreached 005667e0): the
    zones are packed onto the gate as w-aliveai did (kept when they name distinct gate slots, else ranked in their order; for 12 slots and up
    to 12 riders bit for bit the old alive_gate_pack), ranks past the gate take the places behind it; an online grid's zones
    (rx_session_set_start_zones: the picks) are kept when they are distinct places; +0x880 = the slots in use.
- **Online.** The host's gate pick offers the gate's slots, then one place behind the gate per rider past them (`GateCount = min(28,
  max(slots, riders in the round))`, unchanged rule) and now says where the gate ends (`phase.gateSlots`); the pick tiles and the lobby read
  "GATE 5" / "ROW 2 · 1" (`OnlineProtocol.GateLabel`). The messages are the same (one additive field); riders only ride together on one build
  (`SameBuild`), so the protocol version stays 2. The host's MAX RIDERS is no longer capped at an Alive gate's 12.

## What it changes for players today (the core holds 12 riders)

The core races at most 12 riders (12 player seats in MXGame, 12 race indices, 12 standings entries: `RIDER_CAP.md` on w-ridercap; 13 riders
abort at 00552ba5). So:
- **Reflex tracks (16-slot gates):** nothing changes (no race has more riders than slots).
- **Alive tracks offline:** nothing changes (11 AI + the player fit the 12 slots; the grid is bit for bit main's).
- **Alive tracks online:** a session may now take up to 16 riders (it was capped at 12). The 13th .. 16th riders pick (or are given) a place
  in row 2, and every rider's game puts every rider in the same place. Each game still holds 12 riders of its own (itself + 11 others in the
  core; riders 13 .. 16 of the field are drawn as puppets, as on Reflex tracks today).
- **Custom tracks** whose gate has fewer than 12 slots now load (before: the core stopped) and race with the riders past the gate behind it;
  a gate of 13 .. 15 slots loads too.
- When the rider cap rises, Reflex's 16-slot gates get rows behind them automatically (up to 28 riders).

## Measured (headless, `grid_rows_test`, 3 laps, AI 95, MX class 2, the Alive defaults 110 / 130 %)

Placement on all 22 Alive race levels (12 riders, zones 0, 3 .. 9 on the gate, 12 .. 15 behind it: the row a 16-rider online race uses):
every place has a clear spot. Moved for the level: the Farm family (a 1.2 ft rail 11 .. 13 ft behind the slots: the row stands at 7.2 ..
8.2 ft, between the gate's bikes); Northwest / Southwest (a raised 1.27 ft start pad ending ~7.6 ft behind the slots: the row stands at 13.2
.. 13.5 ft, behind the pad's edge with 2 ft of run-up; measured: at 12 ft one rider stalled 3 s at the edge, at 13.5 none); the middle lane
of Tropic, Southwest, SX01 / SX02 (a post / tower leg in the double middle gap: moved 2.9 .. 3.8 ft across); JS7 and Farm's middle lane (a
starter's dog house / tower in the gap, one-sided collision) has no clear spot and is the row's last lane.
Before the search (every place at the plain depth) row-2 bikes on Farm straddled the rail and were thrown 60 .. 110 ft into the air at GO;
on JS7 the middle rider stuck against the dog house; behind Farm's rail riders stuck.

Start (20 s, all 22 levels): every row-2 rider >= 72 ft from its place 6 s after GO (most 130 .. 170 ft; row 1 120 .. 260), longest
stall <= 3.8 s, no crash of the core.

Full races, 11 AI each (`base` = all 11 AI on the gate, the game today; `row2` = 7 AI on the gate + 4 behind it):

| Level | run | finished | crashes / rider | longest stall s | row-2 AI: mean place, crashes / rider, >= ft at GO + 6 s |
|---|---|---:|---:|---:|---|
| NAT_Farm | base | 11 / 11 | 3.55 | 2.2 | |
| NAT_Farm | row2 | 11 / 11 | 2.55 | 3.8 | 9.5, 5.00, 138 |
| NAT_Northwest | base | 11 / 11 | 3.45 | 1.4 | |
| NAT_Northwest | row2 | 11 / 11 | 3.18 | 3.1 | 8.0, 2.75, 132 |
| NAT_Tropic | base | 11 / 11 | 3.09 | 1.5 | |
| NAT_Tropic | row2 | 11 / 11 | 2.64 | 1.2 | 8.2, 3.50, 168 |
| ST_Farm | base | 11 / 11 | 1.82 | 5.1 | |
| ST_Farm | row2 | 11 / 11 | 1.36 | 4.0 | 6.8, 1.00, 148 |
| SX_DLC_SX01 | base | 11 / 11 | 4.00 | 2.5 | |
| SX_DLC_SX01 | row2 | 11 / 11 | 4.64 | 4.1 | 8.5, 6.75, 149 |
| NAT_DLC_JSComp_A | base | 11 / 11 | 5.45 | 2.1 | |
| NAT_DLC_JSComp_A | row2 | 11 / 11 | 3.82 | 1.4 | 7.8, 4.00, 156 |
| NAT_Farm, gate cut to 8 slots | 8 + 4 (ranked) | 11 / 11 | 3.45 | 1.5 | 5.0, 4.00, 157 |
| NAT_Concord, gate cut to 8 slots | 8 + 4 (ranked) | 11 / 11 | 3.45 | 1.1 | 6.2, 2.75, 150 |
| NAT_Concord | base | 11 / 11 | 2.27 | 2.2 | |

(The cut gates use the tools-only hook `rx_grid_rows_test_cap`: the offline path of a race with more riders than slots, ranked zones, on an
Alive and a Reflex gate.) Row-2 riders start behind and finish behind on average (mean place 5 .. 9.5 of 12), as a second row does. Their
crashes per rider are higher than the gate's riders' on Farm (5.0 vs 1.1 in the same race) and SX01 (6.75 vs 3.4): they ride in the pack
for the first laps (4 riders a sample; the field's totals stay in the levels' usual range, ALIVE_RACING.md 4). Every race: all AI
finished, no stall over 4.1 s.

Unchanged (byte-identical tool output against main's core): 12-rider races on NAT_Farm (110 %), NAT_Concord, SX_DLC_SX01 (130 %), a
7-rider NAT_Farm (alive_race_test, 3 laps); net_remote_test solo and live (two cores, start zones armed) identical F lines; alive_gate_test
NAT_Farm identical.

## Past the cap (open)

- The rider cap is 12 (w-ridercap). Rows behind a 16-slot gate need 17+ riders.
- 28 places: past it the slot table must move host-side (the driver's +0x144 pointer and the slot's +0x28 users are all the race reads;
  `race_activity.c`'s debug camera key walks +0x880 slots).
- The grid pass 0075f890 (original) steps the races' zones by round(16 / rows): 33+ rows make the step 0 and `16 % 0` faults (RIDER_CAP.md);
  17 .. 32 rows wrap zones past 15 onto taken ones, which grid_rows' ranking already handles.
- Online: `GateCount` is capped at 28 (`MaxGridPlaces`); `StartZones` accepts 0 .. 27.

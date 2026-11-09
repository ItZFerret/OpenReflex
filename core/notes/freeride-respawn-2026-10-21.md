# Free Ride respawn — 2026-10-21

A 40-second stationary Gibson player baseline reached the unported MXActivityFreeRide::vf19 at 00561650, frame 1158. The earlier ten-second throttle ride had not reached this path. The failure is retained in builds/sol_materials/logs/player_baseline_gibson.log.

`game/race_freeride_respawn.c` now implements the original 00561650..00561815 instruction path: find the rider in the racing/waiting list; find its roam behavior; walk forbidden nodes and section links; preserve the original node for an empty successor section; project the node position plus five metres onto the original base terrain; return the node forward axis and world up. Sequential x87 direction copies quiet signaling NaNs. All callees are existing native owners. No emulator, binary execution, respawn guess or asset reimport was added.

An adapter was added to `oracle/race_activity.cpp`; it has not been exercised in a new oracle comparison. A second agent reviewed the port against the full disassembly. `tools/freeride_respawn_test.c` passes eleven source-evidenced branch fixtures, including early exits, waiting-list selection, class-specific exclusions, terminal and cross-section branches, empty successors, canaries, NaNs and output overlap. Its terrain fixture has no base grid and verifies the unsnapped +5 path. Actual terrain projection remains an existing separately ported dependency.

Central core rebuild and the fixture passed. Player retry results belong in the session handoff; do not treat this fixture or the adapter's existence as full independent oracle acceptance.

// race_champ_probe.cpp -- oracle stimulus for the championship race activity classes (core/game/race_champ.c). No reference world
// holds a championship activity: the PC game's career tables have no "Championship" series (every Series row is a Tour series with
// the plain activity types), and the arcade front end cannot name one (SetupEvent keeps the EventType in a 16-byte buffer, the
// class names are longer). The classes exist in the executable (they are the console game's championship sub events), so the
// original game can still be made to build and run one: the hooks below change what the front end tells the world.
//
// ChampTypeHook (a "thunk"-group hook: never verified, always on when selected): FUN_005964b0, the front end's activity-name -> type
// map (cdecl char*; the 62 names at 0x009d2220, 0x3e when none), answers another type for the arcade event's own. The event keeps
// its level, track and rules (a National event on NAT_Concord) while the activity table's row, the lookups of SetupEvent /
// StartEvent and the players' requests all see the chosen type, so the world's factory (0055ef80) builds the championship class and
// the game's own loops start (vf1, vf2, vf4, vf5) and run it. The one caller that keeps the event's own type is 005b6a30, the
// series manager's current event type, which a championship type consults for the base type it stands in for.
//   REFLEX_PORT=rx_ChampTypeHook,rx_ChampVf5Pre|rx_ChampVf5WpPre,... RX_CHAMP_TYPE=<activity type, decimal> [RX_CHAMP_FROM=<the
//   type to replace, default 0; 6 for a WaypointRace event>]
//
// ChampVf5Pre / ChampVf5WpPre: two things the console flow does elsewhere and the arcade event does not.
//  - The race classes' vf5 (0057e0b0 / 0057e410) do not load the racing-line track (.tdf07) that MXActivityRaceStub::vf5 loads for
//    the plain races, so the activity has no track block and its drivers start without a track cursor (the original stops in
//    RaceDriver::vf3). The first ChampTypeHook call patches slot 5 of the five race-class vtables to ChampVf5Pre (an unmapped
//    address), which loads that track with the core's own loader (the National audio kind) and continues at the class's own vf5
//    (a tail jump: the original code, or the port installed at that address). StartEvent also leaves NumLaps = -1 in the row of a
//    type it did not set up as a lap race (the race would end at its first lap): 3.
//  - The waypoint classes (practice, main event) share MXActivityWaypointRace::vf5, whose Activity::vf5 loads the area's subscene
//    (the gates) only for the types up to the intro's (type <= 0x21): the original stops on a division by zero without gates.
//    ChampVf5WpPre (slot 5 of their vtables) loads the area with the core's Activity::vf5 on the intro's type first.
// Both run outside every verified call (the class's own vf5 is the verified call). The intro classes (0x1f, 0x21) stop in the
// original's own scenario code (the cast and the podium the series sequence sets up): not stimulated.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include "../game/build_activity.h"
using namespace reflex_port;

namespace {
constexpr uint32_t kVf5Native = 0xefff0f50;
constexpr uint32_t kVf5WpNative = 0xefff0f60;
constexpr uint32_t kRaceVtables[5] = {0x00943654, 0x00943754, 0x00943854, 0x00943954, 0x00943a54};
constexpr uint32_t kWpVtables[2] = {0x00943d14, 0x00943e14};   /* the waypoint practice and main event */
bool g_patched = false;
void patch_vtables(reflex_vm::Machine& m) {
    if (g_patched) return;
    g_patched = true;
    for (uint32_t vt : kRaceVtables) {
        m.mem.set_readonly(vt & ~0xfffu, 0x2000, false);   /* .rdata */
        m.mem.write(vt + 4 * 5, 4, kVf5Native);
    }
    for (uint32_t vt : kWpVtables) {
        m.mem.set_readonly(vt & ~0xfffu, 0x2000, false);
        m.mem.write(vt + 4 * 5, 4, kVf5WpNative);
    }
}
}  // namespace

RX_PORT(0x005964b0, ChampTypeHook, "thunk", 0) {
    const char* to = std::getenv("RX_CHAMP_TYPE");
    if (!to) return reflex_vm::RUN_ORIGINAL;
    patch_vtables(m);
    const uint32_t from = std::getenv("RX_CHAMP_FROM") ? uint32_t(std::atoi(std::getenv("RX_CHAMP_FROM"))) : 0;
    const uint32_t caller = uint32_t(m.mem.read(m.r[4], 4));
    if (caller >= 0x005b6a30 && caller < 0x005b6b00) return reflex_vm::RUN_ORIGINAL;   /* the series manager's current type */
    char name[64] = {};
    for (uint32_t i = 0; i < sizeof name - 1; i++) { name[i] = char(m.mem.read(m.arg(0) + i, 1)); if (!name[i]) break; }
    uint32_t type = 0x3e;
    for (uint32_t i = 0; i < 0x3e; i++) {
        const uint32_t p = uint32_t(m.mem.read(0x009d2220 + 4 * i, 4));
        char cand[64] = {};
        for (uint32_t k = 0; k < sizeof cand - 1; k++) { cand[k] = char(m.mem.read(p + k, 1)); if (!cand[k]) break; }
        if (strcasecmp(name, cand) == 0) { type = i; break; }
    }
    if (type == from) type = uint32_t(std::atoi(to));
    m.r[0] = type;
    return m.ret(0);
}

RX_PORT(0xefff0f50, ChampVf5Pre, "thunk", 0) {
    const uint32_t act = m.r[1], vt = uint32_t(m.mem.read(act, 4));
    const uint32_t target = (vt == 0x943654 || vt == 0x943854) ? 0x0057e0b0 : 0x0057e410;
    const uint32_t world = uint32_t(m.mem.read(act + 0x234, 4));
    if (m.mem.read(act + 0xff070, 4) == 0xffffffffu) m.mem.write(act + 0xff070, 4, 3);
    if (m.mem.read(act + 0xbb0, 4) == 0 && m.mem.read(act + 0x238, 4) != 0) {
        std::fprintf(stderr, "[champ] track load for activity %08x (type %u)\n", act, uint32_t(m.mem.read(act + 0x23c, 4)));
        rx_track_load(world + 0x20030, act + 0x890, 0, uint32_t(m.mem.read(act + 0x220, 4)));
    }
    m.r[1] = act;   /* the tail jump keeps the thiscall's ECX (the loader ran original code) */
    return target;
}

RX_PORT(0xefff0f60, ChampVf5WpPre, "thunk", 0) {
    const uint32_t act = m.r[1];
    if (m.mem.read(act + 0x238, 4) != 0 && m.mem.read(act + 0xec8, 4) == 0) {
        const uint32_t type = uint32_t(m.mem.read(act + 0x23c, 4));
        std::fprintf(stderr, "[champ] area load for activity %08x (type %u)\n", act, type);
        m.mem.write(act + 0x23c, 4, 0x21);
        rx_activity_vf5(act);
        m.mem.write(act + 0x23c, 4, type);
    }
    m.r[1] = act;
    return 0x00578500;
}

// ChampInvalidParam / ChampStateTrace: diagnostics with RX_CHAMP_TRACE=1 (return addresses of the game's frames on the stack).
// _invalid_parameter_noinfo (msvcr90 0x7858c949) is where the CRT ends the process for a checked-container misuse of a stimulus
// that leaves the original's defined paths; Activity_SetState (005586c0) shows who moves the activity to which state.
static void stack_trace(reflex_vm::Machine& m, int limit) {
    int n = 0;
    for (uint32_t a = m.r[4], i = 0; i < 2048 && n < limit; a += 4, i++) {
        const uint32_t v = uint32_t(m.mem.read(a, 4));
        if (v > 0x401000 && v < 0x916000) { std::fprintf(stderr, " %08x", v); n++; }
    }
    std::fprintf(stderr, "\n");
}
RX_PORT(0x7858c949, ChampInvalidParam, "thunk", 0) {
    if (!std::getenv("RX_CHAMP_TRACE")) return reflex_vm::RUN_ORIGINAL;
    std::fprintf(stderr, "[champ] _invalid_parameter_noinfo from:");
    stack_trace(m, 24);
    return reflex_vm::RUN_ORIGINAL;
}
RX_PORT(0x005586c0, ChampStateTrace, "thunk", 0) {
    if (!std::getenv("RX_CHAMP_TRACE")) return reflex_vm::RUN_ORIGINAL;
    std::fprintf(stderr, "[champ] SetState act %08x %u -> %u from", m.arg(0), uint32_t(m.mem.read(m.arg(0) + 0x21c, 4)), m.arg(1));
    stack_trace(m, 6);
    return reflex_vm::RUN_ORIGINAL;
}

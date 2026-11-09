// race_stunt.cpp -- oracle adapters for core/game/race_stunt.c (MXActivityRace's stunt handlers on the stunts channel 0x00a94304).
// ECX = the activity, the player as the first stack argument (then the stunt descriptor, then the stunt type for
// "PhysicsStuntBegin"). The handlers write memory only; their callers (the event proxy 0058f270 / the 00588130 / 005881f0 forms)
// read no register result.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_stunt.h"
}
using namespace reflex_port;

// 00565710 "PhysicsStuntBegin" (RET 0xc).
RX_PORT(0x00565710, Race_PhysicsStuntBegin, "rx_race", 0) {
    rx_race_physics_stunt_begin(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 00565770 "StuntBegin" (RET 8).
RX_PORT(0x00565770, Race_StuntBegin, "rx_race", 0) {
    rx_race_stunt_begin(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 005657d0 "StuntFailed" (RET 8).
RX_PORT(0x005657d0, Race_StuntFailed, "rx_race", 0) {
    rx_race_stunt_failed(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}

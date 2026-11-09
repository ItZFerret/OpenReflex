// race_rule_cannonball.cpp -- oracle adapters for core/game/race_rule_cannonball.c (group rx_race).
#include "oracle.h"
extern "C" {
#include "../game/race_rule_cannonball.h"
}
using namespace reflex_port;

// 00519ef0 RuleCannonball::vf1 (thiscall driver; RET 4)
RX_PORT(0x00519ef0, RuleCannonball_vf1, "rx_race", 0) { rx_rule_cannonball_vf1(m.r[1], m.arg(0)); return m.ret(4); }

// 00519f50 RuleCannonball::vf2 (thiscall). Void.
RX_PORT(0x00519f50, RuleCannonball_vf2, "rx_race", 0) { rx_rule_cannonball_vf2(m.r[1]); return m.ret(0); }

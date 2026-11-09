// race_challenge.cpp -- oracle adapters for core/game/race_challenge.c (group rx_race): the profile's challenge awards.
#include "race_oracle.h"
extern "C" {
#include "../game/race_challenge.h"
}
using namespace reflex_port;

// 0054e670 (stdcall player, kind, medal, value; RET 0x10): the Free Ride jump / discovery award
RX_PORT(0x0054e670, Challenge_Award, "rx_race", 0) { rx_challenge_award(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
// 004ea0a0 (stdcall type id, location, track; RET 0xc) -> EAX
RX_PORT(0x004ea0a0, Challenge_EventLocation, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_challenge_event_location(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0xc); }
// 004ea6c0 (EAX = profile; stdcall etl, &bronze, &silver, &gold, &param; RET 0x14)
RX_PORT(0x004ea6c0, Challenge_ProfileRead, "rx_race", 0) { rx_challenge_profile_read(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
// 004ea3a0 (EAX = profile; stdcall etl, bronze, silver, gold, param; RET 0x14)
RX_PORT(0x004ea3a0, Challenge_ProfileWrite, "rx_race", 0) { rx_challenge_profile_write(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
// 006d8390 (EDI = the highlight tracking, ESI = the profile; plain ret)
RX_PORT(0x006d8390, Challenge_CareerProgress, "rx_race", 0) { rx_challenge_career_progress(m.r[7], m.r[6]); return m.ret(0); }
// 00554d40 (ECX = kind, EDX = medal, ESI = the event manager; stdcall player; RET 4)
RX_PORT(0x00554d40, Challenge_AwardEvent, "rx_race", 0) { rx_challenge_award_event(m.r[1], m.r[2], m.r[6], m.arg(0)); return m.ret(4); }
// 0055ec70 / 0055ed20 (thiscall activity (unused); stdcall new, old; RET 8) -> EAX: -1 / 0 / +1
RX_PORT(0x0055ec70, Challenge_CompareLower, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_challenge_compare_lower_better(m.r[1], m.arg(0), m.arg(1))); return m.ret(8); }
RX_PORT(0x0055ed20, Challenge_CompareHigher, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_challenge_compare_higher_better(m.r[1], m.arg(0), m.arg(1))); return m.ret(8); }

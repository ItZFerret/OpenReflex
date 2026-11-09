// career_activity.cpp -- oracle adapter for core/game/career_activity.c (group rx_race).
#include "oracle.h"
extern "C" {
#include "../game/career_activity.h"
}
using namespace reflex_port;

// 005b6a30 (stdcall manager; RET 4) -> EAX: the series manager's current event type (the authored event's +0x1c, else the UI property "EventType" parsed).
RX_PORT(0x005b6a30, CareerCurrentEventType, "rx_race", OUT_EAX) { m.r[0] = rx_career_current_event_type(m.arg(0)); return m.ret(4); }

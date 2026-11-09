// race_aidata.cpp -- oracle adapters for core/game/race_aidata.c (the AIData tables, group rx_aidata).
//
// The original init thread 0075ed60 calls 0051e350 after the AIData package. Registering the whole chain lets a baton boot verify
// every function per call in layers: with 0051e350 registered the adapter runs the core loader and then the original (whose calls
// reach the registered children); with only the children registered the original loader calls their adapters.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/build_asset.h"
#include "../game/race_aidata.h"
#include "../game/race.h"
#include "../game/race_beh1.h"
}
using namespace reflex_port;

RX_PORT(0x0051e350, AiDataLoad, "rx_aidata", 0) { rx_aidata_load(); return m.ret(0); }
RX_PORT(0x0051eac0, AiDataBehaviors, "rx_aidata", 0) { rx_aidata_behaviors(m.r[7]); return m.ret(0); }
RX_PORT(0x0051e430, AiDataAbilities, "rx_aidata", 0) { rx_aidata_abilities(m.r[6]); return m.ret(0); }
RX_PORT(0x0051e7b0, AiDataOverrides, "rx_aidata", 0) { rx_aidata_overrides(m.r[6]); return m.ret(0); }
RX_PORT(0x0051e1d0, AiDataAbilityDefaults, "rx_aidata", 0) { rx_aidata_ability_defaults(m.r[0], m.r[1]); return m.ret(0); }

/* the behaviour vf1s: thiscall (ECX = reader), one stack argument (the row) */
#define AIDATA_VF1(addr, Name, fn) \
    RX_PORT(addr, Name, "rx_aidata", 0) { fn(m.r[1], m.arg(0)); return m.ret(0); }
AIDATA_VF1(0x0051eed0, AiDataVfGeneral, rx_aidata_vf_general)
AIDATA_VF1(0x0051f190, AiDataVfAirLevel, rx_aidata_vf_airlevel)
AIDATA_VF1(0x005203b0, AiDataVfAvoidObjects, rx_aidata_vf_avoidobjects)
AIDATA_VF1(0x00521910, AiDataVfBoundary, rx_aidata_vf_boundary)
AIDATA_VF1(0x00522bd0, AiDataVfBrakeTurn, rx_aidata_vf_braketurn)
AIDATA_VF1(0x00524630, AiDataVfHandleBraking, rx_aidata_vf_handlebraking)
AIDATA_VF1(0x00526e80, AiDataVfCenter, rx_aidata_vf_center)
AIDATA_VF1(0x00528e80, AiDataVfOfftrack, rx_aidata_vf_offtrack)
AIDATA_VF1(0x00529920, AiDataVfOverrideSpline, rx_aidata_vf_overridespline)
AIDATA_VF1(0x00531650, AiDataVfSeparate, rx_aidata_vf_separate)
AIDATA_VF1(0x00533730, AiDataVfSplitSection, rx_aidata_vf_splitsection)
AIDATA_VF1(0x005341d0, AiDataVfTakeLine, rx_aidata_vf_takeline)
AIDATA_VF1(0x00534e30, AiDataVfSuckRacing, rx_aidata_vf_suckracing)

/* the value readers: ESI = reader, EDX = the value-group row, ECX = the reader (the vf1's this) */
#define AIDATA_READER(addr, Name, fn) \
    RX_PORT(addr, Name, "rx_aidata", 0) { fn(m.r[6], m.r[2]); return m.ret(0); }
AIDATA_READER(0x0051ef80, AiDataReadGeneral, rx_aidata_read_general)
AIDATA_READER(0x0051f240, AiDataReadAirLevel, rx_aidata_read_airlevel)
AIDATA_READER(0x005204b0, AiDataReadAvoidObjects, rx_aidata_read_avoidobjects)
AIDATA_READER(0x00520620, AiDataReadAvoidSphere, rx_aidata_read_avoidsphere)
AIDATA_READER(0x00520790, AiDataReadSuperAvoid, rx_aidata_read_superavoid)
AIDATA_READER(0x00520900, AiDataReadAvoidEco, rx_aidata_read_avoideco)
AIDATA_READER(0x005219c0, AiDataReadBoundary, rx_aidata_read_boundary)
AIDATA_READER(0x00522c80, AiDataReadBrakeTurn, rx_aidata_read_braketurn)
AIDATA_READER(0x005246e0, AiDataReadHandleBraking, rx_aidata_read_handlebraking)
AIDATA_READER(0x00526f30, AiDataReadCenter, rx_aidata_read_center)
AIDATA_READER(0x00528f30, AiDataReadOfftrack, rx_aidata_read_offtrack)
AIDATA_READER(0x005299d0, AiDataReadOverrideSpline, rx_aidata_read_overridespline)
AIDATA_READER(0x00531700, AiDataReadSeparate, rx_aidata_read_separate)
AIDATA_READER(0x005337e0, AiDataReadSplitSection, rx_aidata_read_splitsection)
AIDATA_READER(0x00534280, AiDataReadTakeLine, rx_aidata_read_takeline)
AIDATA_READER(0x00534ee0, AiDataReadSuckRacing, rx_aidata_read_suckracing)

/* 006cc1a0 "VehicleTakeoff": thiscall (ECX = the stunt system), one stack argument (the vehicle) */
RX_PORT(0x006cc1a0, StuntVehicleTakeoff, "rx_aidata", 0) { rx_stunt_vehicle_takeoff(m.r[1], m.arg(0)); return m.ret(4); }

// ---- the AI-frame functions this unit had to port once the AIData tables let the riders race (group rx_race: the five
// reference worlds reach them with their AI) ----
RX_PORT(0x00554c40, EventVehicleLanded, "rx_race", 0) {
    rx_event_vehicle_landed(m.r[6], m.arg(0), m.r[1]); return m.ret(4);
}
RX_PORT(0x00526b90, BehaviorCornerLast, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_corner_last(m.r[1], m.r[2], m.arg(0)); return m.ret(4);
}
RX_PORT(0x00526bf0, BehaviorCornerLength, "rx_race", OUT_XMM0) {
    const float v = rx_behavior_corner_length(m.r[0], m.r[1], m.arg(0));
    m.xmm[0].lo = rx_oracle::bits(v); m.xmm[0].hi = 0; return m.ret(4);
}
RX_PORT(0x005380f0, BehaviorCornerDirection, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_corner_direction(m.r[0], m.r[6]); return m.ret(0);
}

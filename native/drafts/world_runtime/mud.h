// Mud build-up on vehicles and riders (mud.cpp; _re/notes/mud.md, _re/names/79_mud.tsv), read from the running game.
// The game keeps one mud record per vehicle in a static table of 12 (0xa96d90, stride 0x40):
//   +0x00 usr_mud_amount  float4 (four equal lanes; the shaders read .w)
//   +0x10 usr_mud_wetness float4
//   +0x20 usr_mud_color   float4 (the level MudManager's MudR/G/B/A / 255, darkened while wet)
//   +0x30 owner: the vehicle V = [P+0x1ac] (0 = free)
// MudAndNumberCallback::vf0 (0054a2e0) allocates it when the rider is inserted (MudRecord_Alloc 00408fb0 -> MudRecord_Bind 004089d0):
// the vehicle model's and its rider's ([V+0x1a0] = [P+0x1b4]) shader-constant overrides point at the three float4s, so every
// material of the bike and the gear whose pixel shader declares those constants gets them. MXGame::Update (00757fb0) runs
// MudRecord_Update (00408c50) on every bound record each time its 0.1 s accumulator (0xa89bc4) passes 0.1.
#pragma once
#include <cstdint>

namespace reflex_vm {
class Memory;
constexpr uint32_t MUD_RECORDS = 0xa96d90, MUD_RECORD_STRIDE = 0x40, MUD_RECORD_COUNT = 12;

// reflex_world.h ReflexMudState, same layout.
struct MudState {
    int32_t slot, record;
    uint32_t vehicle, rider;
    float amount[4], wetness[4], color[4];
};
// P: the player slot (G+0x7460+k*0x12a0). False when the slot has no vehicle or its vehicle no mud record. Reads only.
bool mud_state(Memory& m, uint32_t P, int32_t slot, MudState& out);
}  // namespace reflex_vm

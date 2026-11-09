// The vehicle classes a race slot can drive (RTTI _re/names/00_rtti_labels.tsv; _re/notes/atv.md, _re/names/78_atv.tsv).
// The player slot P (G+0x7460+k*0x12a0) holds the vehicle V = [P+0x1ac]: RavenVehicleMotocross (vtable 0x93cd84) for the
// motocross bikes (VehicleType MX Mini / MX Lite / MX, FactoryName "MotocrossBike") or RavenVehicleATV (0x93d0ec) for the
// ATVs ("Atv"). Its physics is a RavenSimulation subclass at [V+0x234] (also [V+0x16d4] MX / [V+0x1700] ATV):
// RavenSimulationMotocross (0x93c8a4) with 4 bodies (S+0xec: frame, rider, front wheel, rear wheel) or RavenSimulationATV
// (0x93d054) with 6 (frame, rider and the four wheels). The base RavenSimulation body arrays (S+0xf0 + 0xa0*i positions,
// +0x640 orientations, +0x90 / +0x6d0 the render pose) are shared; the vehicle's render model is [V+0x1328] for both.
#pragma once
#include <cstdint>
#include "vehicles.h"

namespace reflex_vm {
class Memory;
constexpr uint32_t SIM_MOTOCROSS_VTABLE = 0x93c8a4, SIM_ATV_VTABLE = 0x93d054;
constexpr uint32_t VEHICLE_MOTOCROSS_VTABLE = 0x93cd84, VEHICLE_ATV_VTABLE = 0x93d0ec;
constexpr uint32_t SIM_MOTOCROSS_BODIES = 4, SIM_ATV_BODIES = 6;
// A live vehicle simulation: one of the factory's classes (vehicles.h: also the trucks, buggies and UTVs) with its own body count
// (other copies of the vtable exist).
inline bool vehicle_sim(uint32_t vtable, uint32_t bodies) { auto* c = vehicle_class_of_sim(vtable); return c && bodies == c->bodies; }
// The gear the HUD shows less one (RavenVehicle*::vf42 returns it + 1): RavenSimulationMotocross +0x8ac0 / RavenSimulationATV +0x96f0;
// the engine speed (vf47: rad/s, the caller multiplies by 9.55 for rpm): +0x8abc / +0x96ec.
// (vehicles.h for the other classes.)
inline uint32_t sim_gear_offset(uint32_t vtable) { auto* c = vehicle_class_of_sim(vtable); return c ? c->gear : 0x8ac0; }
inline uint32_t sim_engine_offset(uint32_t vtable) { auto* c = vehicle_class_of_sim(vtable); return c ? c->engine : 0x8abc; }

// One slot's vehicle (reflex_world.h ReflexVehicleInfo, same layout).
struct VehicleInfo {
    int32_t slot, type, bodies, model_nodes;
    uint32_t vehicle, sim;
    int32_t gear;
    float rpm, speed;
    float render_position[6][4], render_orientation[6][4];
};
// P: the player slot (G+0x7460+k*0x12a0). False when the slot has no vehicle.
bool vehicle_info(Memory& m, uint32_t P, int32_t slot, VehicleInfo& out);
}  // namespace reflex_vm

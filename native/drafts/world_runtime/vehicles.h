// Every vehicle class the original's factory builds (_re/notes/vehicles.md, _re/names/84_vehicles.tsv; RTTI _re/names/00_rtti_labels.tsv).
// VehicleFactory_CreateVehicle 00542b90 switches on the VehicleType FactoryName's index (names at 0x9ce220: 0 MotocrossBike, 1 Atv,
// 2 MonsterTruck, 3 TrophyTruck, 4 SandRail, 5 Buggy, 6 GolfCart, 7 Rhino) and writes it to V+0x194; 2 / 4 / 6 build nothing (no
// VehicleModel row of those types, no packages). The vehicle's RavenVehicle*::vf28 allocates its RavenSimulation* and stores it at its own
// field and at V+0x234 (the base's simulation pointer); the HUD's gear is vf42 ([S+gear] + 1), the engine speed vf47 ([S+engine] * 9.55).
//   factory  VehicleTypes (MXTables_Default)                vehicle (vtable)               simulation (vtable, size)            bodies  S at  gear    engine
//   0        MX Mini / MX Lite / MX / MX Freestyle            RavenVehicleMotocross 0x93cd84  RavenSimulationMotocross 0x93c8a4     4       16d4  8ac0    8abc
//   1        ATV / ATV Freestyle                              RavenVehicleATV 0x93d0ec        RavenSimulationATV 0x93d054 (0x9bd0)  6       1700  96f0    96ec
//   3        Pro 4 / Pro Lite / Pro 2 Truck, Trophy           RavenVehicleTrophyTruck 0x93cee4 RavenSimulationTrophyTruck 0x93c93c (0x8930) 5 15d0 8664  865c
//   5        Super Buggy, Offroad Buggy                       RavenVehicleBuggy 0x93d2e4      RavenSimulationBuggy 0x93d24c (0x8840) 5      15c0  8650    864c
//   7        UTV, Side-by-side ("Rhino")                      RavenVehicleUTV 0x93d4d4        RavenSimulationUTV 0x93d43c (0x8850)  5       1540  8660    865c
// Bodies (RavenSimulation base, S+0xec; records S+0xf0 + 0xa0 i, orientation +0x640, render pose +0x90 / +0x6d0): MX frame, rider, front,
// rear wheel; ATV frame, rider, front left / right, rear left / right wheel; the four-wheelers with a seated driver (trucks, buggy, UTV)
// the chassis then front left / right, rear left / right wheel (no rider body: the driver's RiderAnimation rides the chassis).
#pragma once
#include <cstdint>

namespace reflex_vm {
struct VehicleClass {
    int32_t factory;                       // V+0x194, the FactoryName index
    uint32_t vehicle_vtable, sim_vtable, bodies;
    uint32_t sim_field, gear, engine;      // V+sim_field = S (also V+0x234); S+gear (+1: the HUD's gear); S+engine (rad/s)
    const char* name;
};
inline constexpr VehicleClass VEHICLE_CLASSES[] = {
    {0, 0x93cd84, 0x93c8a4, 4, 0x16d4, 0x8ac0, 0x8abc, "Motocross"},
    {1, 0x93d0ec, 0x93d054, 6, 0x1700, 0x96f0, 0x96ec, "ATV"},
    {3, 0x93cee4, 0x93c93c, 5, 0x15d0, 0x8664, 0x865c, "TrophyTruck"},
    {5, 0x93d2e4, 0x93d24c, 5, 0x15c0, 0x8650, 0x864c, "Buggy"},
    {7, 0x93d4d4, 0x93d43c, 5, 0x1540, 0x8660, 0x865c, "UTV"},
};
inline const VehicleClass* vehicle_class_of_sim(uint32_t sim_vtable) {
    for (auto& c : VEHICLE_CLASSES) if (c.sim_vtable == sim_vtable) return &c;
    return nullptr;
}
inline const VehicleClass* vehicle_class_of_vehicle(uint32_t vehicle_vtable) {
    for (auto& c : VEHICLE_CLASSES) if (c.vehicle_vtable == vehicle_vtable) return &c;
    return nullptr;
}
}  // namespace reflex_vm

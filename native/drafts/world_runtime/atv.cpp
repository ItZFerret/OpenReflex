// ATVs in the original world (atv.h; _re/notes/atv.md, _re/names/78_atv.tsv): which vehicle class a slot drives and the bodies
// of its simulation, read from the running game.
#include "atv.h"
#include "vm.h"
#include <cstring>

namespace reflex_vm {

bool vehicle_info(Memory& m, uint32_t P, int32_t slot, VehicleInfo& out) {
    std::memset(&out, 0, sizeof out);
    auto u32 = [&](uint32_t a) { return m.mapped(a) ? uint32_t(m.read(a, 4)) : 0u; };
    auto f32 = [&](uint32_t a) { uint32_t v = u32(a); float f; std::memcpy(&f, &v, 4); return f; };
    out.slot = slot; out.type = -1;
    uint32_t V = P ? u32(P + 0x1ac) : 0;
    if (!V) return false;
    uint32_t vvt = u32(V), S = u32(V + 0x234), svt = S ? u32(S) : 0;   // RavenVehicle +0x234: the simulation (also MX +0x16d4 / ATV +0x1700)
    out.vehicle = V; out.sim = S;
    { auto* c = vehicle_class_of_vehicle(vvt); out.type = c ? c->factory : -1; }   // the factory index (vehicles.h): 0 MX, 1 ATV, 3 truck, 5 buggy, 7 UTV
    out.speed = f32(V + 0x150);
    uint32_t model = u32(V + 0x1328);
    out.model_nodes = model ? int32_t(u32(model + 0x10 + 0xc)) : 0;
    if (!S || !vehicle_sim(svt, u32(S + 0xec))) return true;
    out.bodies = int32_t(u32(S + 0xec));
    out.gear = int32_t(u32(S + sim_gear_offset(svt))) + 1;
    out.rpm = f32(S + sim_engine_offset(svt)) * 9.55f;
    for (int i = 0; i < out.bodies && i < 6; i++) {
        uint32_t b = S + 0xf0 + 0xa0 * uint32_t(i);   // RavenSimulation body records: render pose +0x90 (position) / +0x6d0 (orientation)
        m.read_bytes(b + 0x90, out.render_position[i], 16);
        m.read_bytes(b + 0x6d0, out.render_orientation[i], 16);
    }
    return true;
}

}  // namespace reflex_vm

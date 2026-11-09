// Vehicle simulation ports (group "bike"), transliterated instruction by instruction from MXReflex.exe by the bike
// lifter (see native/game/notes/bike.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 0044bfb0 RavenSimulationMotocross_vf23: Slot 23 (ECX = S, stack controller): Simulation_ReadControllerInput into S+1c,
//   mirrors steering / shift axes into the control block.
//   original RavenSimulationMotocross::vf23; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x0044bfb0, RavenSimulationMotocross_vf23, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    esp -= 4; wr32(m, esp, edi);
    edi = (esi + 0x1cu);
    LIFT_CALL(0x4308a0u, 0x44bfc0u);  // Simulation_ReadControllerInput
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x2cu)));
    eax = rd32(m, esi + 0x5c30u);
    wr32(m, eax + 0x40u, fst32(m));
    fpop(m);
    ecx = rd32(m, esi + 0x5c30u);
    edx = rd32(m, esi + 0x4cu);
    wr32(m, ecx + 0x78u, edx);
    eax = rd32(m, esi + 0x5c30u);
    ecx = rd32(m, esi + 0x50u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x7cu, ecx);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0044bff0 RavenSimulationMotocross_vf24: Slot 24 (ECX = S, stack 64-byte input block): RavenSimulation::vf24, mirrors
//   steering into the control block.
//   original RavenSimulationMotocross::vf24; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x0044bff0, RavenSimulationMotocross_vf24, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    esi = ecx;
    LIFT_CALL(0x433380u, 0x44bffdu);  // RavenSimulation::vf24
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x2cu)));
    ecx = rd32(m, esi + 0x5c30u);
    wr32(m, ecx + 0x40u, fst32(m));
    fpop(m);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

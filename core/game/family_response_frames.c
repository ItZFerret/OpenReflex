/* Family call-frame observation around the existing canonical semantic owners.
 * Actual Buggy/Truck clamp bodies share the same prologue and differ in the
 * status field; their wheel limit is distinct from UTV's runtime operand. */
#include "family_response_frames.h"
#include "family_clamp.h"
#include "fx_util.h"
#include "phys_mech.h"
#include <stdlib.h>

static unsigned family_status(rx_addr simulation, rx_addr return_pc, unsigned offset) {
    const uint32_t vt = U32(simulation, 0);
    if (vt == 0x0093d24c && return_pc == 0x004b3520 + offset) return 0x8010;
    if (vt == 0x0093c93c && return_pc == 0x00451030 + offset) return 0x8020;
    abort();
}

void rx_family_response_clamp_frame(rx_addr simulation, rx_addr expected_sp) {
    const rx_addr entry = rx_sp();
    const unsigned status = family_status(simulation, U32(entry, 0), 0x19);
    if (U32(entry, 4) != simulation || expected_sp != entry + 8) abort();
    /* Both 4b33e0 and 450ef0: EBP=entry-4, align16, SUB14, save
     * EBX/ESI/EDI. The semantic owner writes the real comparison mask at B10. */
    const rx_addr ebp = entry - 4, base = (ebp & ~15u) - 0x20;
    SU32(ebp, 0, fx_reg(FX_EBP));
    SU32(base, 8, fx_reg(FX_EBX));
    SU32(base, 4, fx_reg(FX_ESI));
    SU32(base, 0, fx_reg(FX_EDI));
    rx_family_clamp_angular_velocity(simulation, 0x00935d28, status);
    if (rx_sp() != entry) abort();
    const int32_t count = I32(simulation, 0xec);
    fx_set_reg(FX_EAX, simulation);
    if (count > 1) {
        fx_set_reg(FX_ECX, simulation + 0x740 + 160u * (uint32_t)count);
        fx_set_reg(FX_EDX, (uint32_t)count);
    } else {
        fx_set_reg(FX_ECX, U32(base, 0x14));
        fx_set_reg(FX_EDX, 1);
    }
    rx_set_sp(expected_sp);
}

void rx_family_response_velocity_frame(rx_addr mechanism, rx_addr vector, rx_addr expected_sp) {
    const rx_addr entry = rx_sp();
    if (mechanism < 0xd0) abort();
    (void)family_status(mechanism - 0xd0, U32(entry, 0), 0x8e);
    if (expected_sp != entry + 4 || fx_reg(FX_EDX) != mechanism ||
        fx_reg(FX_ESI) != vector || vector != 0x00d61670) abort();
    /* Shared 85c4b0 leaf: no stack frame or child calls. Its actual count
     * controls EAX/ECX; EAX remains untouched when the count is nonpositive. */
    phys_mech_set_velocity(mechanism, vector);
    if (rx_sp() != entry) abort();
    const int32_t count = I32(mechanism, 0x1c);
    fx_set_reg(FX_ECX, count > 0 ? (uint32_t)count : 0);
    if (count > 0) fx_set_reg(FX_EAX, mechanism + 0x90 + 160u * (uint32_t)count);
    rx_set_sp(expected_sp);
}

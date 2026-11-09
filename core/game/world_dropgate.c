/* world_dropgate.c -- DropGate (the start gate, vtable 0x0095fc24; the vf19 update and the swing are anim_ik_ent.c's): its activity handlers: "Activity Init" 006bdb50 subscribes it
 * to the activity's "Countdown Step" (handler 006bed90) unless the activity is a free ride (kind 8), "Activity UnInit" 006bdb90 unsubscribes it (006c3d90) and resets the gate
 * (006bdbe0). Gate: +0x160 its collision part (+4 the body, +8 the index: the part record's +0x38 byte = the collision flag), +0x1a0 state (0 up, 4 down), +0x1b0 the
 * up pose matrix (4x4), +0x30 the entity matrix, +0x1f0 the down angle, +0x1f8 the angle swung, +0x20c / +0x210 the activity channel and the activity. */
#include "build_activity_util.h"
#include "world_dropgate.h"
#include "phys_vec.h"
#include "world_misc.h"
#include "race_activity_misc.h"

/* 006bdb50 (thiscall gate, stack activity; RET 4): for an activity of kind != 8 (a free ride): +0x210 = the activity, +0x20c = its channel (+0x290), and the gate subscribed
 * to "Countdown Step" (006c3cd0: EAX = the channel; stack gate, 006bed90, 0) */
void rx_drop_gate_activity_init(rx_addr gate, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    if (U32(act, 0x23c) != 8) {
        SU32(gate, 0x210, act);
        SU32(gate, 0x20c, U32(act, 0x290));
        { const rx_addr sp_ = rx_sp(); SU32(sp_, -12, gate); SU32(sp_, -8, 0x006bed90); SU32(sp_, -4, 0); }
        ACT_CALL(3, rx_event_register_countdown_step(U32(act, 0x290), gate, 0x006bed90, 0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006bdb50, rx_drop_gate_activity_init)

/* 006c3d90 (EAX = the channel; stack entity, callback, argument; RET 0xc): the "Countdown Step" listener {vtable 0x009603f4, 0, the signature name (0x009cf5b4), entity, callback,
 * argument} leaves the channel (00592ef0) */
void rx_event_unregister_countdown_step(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    ACT_FRAME(L, 0x1c, 1);
    const rx_addr K = L + 8;
    SU32(L, -4, 0x00941bdc); SU32(L, -8, channel);
    rx_addr ch;
    ACT_CALLR(ch, 2, rx_world_registry_find(channel, 0x00941bdc));
    SU32(K, 0xc, entity);
    SU32(K, 4, 0);
    SU32(K, 0, 0x009603f4);
    SU32(K, 0x10, cb); SU32(K, 0x14, arg);
    SU32(K, 8, crt_type_info_name(0x009cf5b4, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(ch, K));
    ACT_END(L);
}
RX_FUNCTION(0x006c3d90, rx_event_unregister_countdown_step)

/* 006bdb90 (thiscall gate, stack activity; RET 4): the gate's own activity (+0x210) of kind != 8: unsubscribed from its channel (+0x20c) and reset (006bdbe0: EAX = the activity,
 * ECX = the gate) */
void rx_drop_gate_activity_uninit(rx_addr gate, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    if (U32(gate, 0x210) == act && U32(act, 0x23c) != 8) {
        { const rx_addr sp_ = rx_sp(); SU32(sp_, -12, gate); SU32(sp_, -8, 0x006bed90); SU32(sp_, -4, 0); }
        ACT_CALL(3, rx_event_unregister_countdown_step(U32(gate, 0x20c), gate, 0x006bed90, 0));
        ACT_CALL(0, rx_drop_gate_reset(gate, act));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006bdb90, rx_drop_gate_activity_uninit)

/* 006bed90 (thiscall gate; stack activity, step; RET 8): "Countdown Step" 0 of a gate that is up (+0x1a0 == 0): state 1 (dropping), the drop angle +0x1f8 and +0x204 = 0 */
void rx_drop_gate_countdown_step(rx_addr gate, rx_addr act, uint32_t step) {
    (void)act;
    if (U32(gate, 0x1a0) == 0 && step == 0) {
        SU32(gate, 0x1a0, 1);
        SF32(gate, 0x1f8, 0.0f);
        SF32(gate, 0x204, 0.0f);
    }
}
RX_FUNCTION(0x006bed90, rx_drop_gate_countdown_step)

/* 006bdbe0 (EAX = the activity, ECX = the gate): the gate put to its resting pose: an activity of kind != 8, a mode (+0x238) other than 0 / 6 and no session result (+0xfe8): the collision
 * part on, up (+0x1a0 = 0, +0x1f8 = 0.0) and the up pose (+0x1b0) copied to the entity matrix (+0x30); else the collision part off and the gate down (+0x1a0 = 4, +0x1f8 = the down
 * angle +0x1f0): the up pose turned about its x axis by the down angle (sine / cosine polynomials of 0x00935d3c ..., the zero products kept) into +0x30 */
void rx_drop_gate_reset(rx_addr g, rx_addr act) {
    ACT_FRAME_EBP(L, 16, 0xe4, 3);
    const int upright = U32(act, 0x23c) != 8 && U32(act, 0x238) != 0 && U32(act, 0x238) != 6 && U32(act, 0xfe8) == 0;
    const rx_addr part = U32(g, 0x160);
    if (part != 0) {
        const rx_addr body = U32(part, 4);
        const uint32_t idx = U32(part, 8);
        if (idx >= (uint32_t)((int32_t)(U32(body, 0x19c4) - U32(body, 0x19c0)) >> 6)) crt_invalid_parameter_noinfo();
        SU8(U32(body, 0x19c0), 0x40u * idx + 0x38, upright ? 1 : 0);
    }
    if (upright) {
        SU32(g, 0x1a0, 0);
        SF32(g, 0x1f8, 0.0f);
        for (uint32_t k = 0; k < 16; k++) SU32(g, 0x30 + 4 * k, U32(g, 0x1b0 + 4 * k));
        ACT_END(L);
        return;
    }
    float xmm0 = 0, xmm1 = 0, xmm2 = 0, xmm3 = 0, xmm4 = 0, xmm5 = 0, xmm6 = 0, xmm7 = 0;
    int cf;
    int32_t ecx;
    xmm0 = F32(g, 0x1f0);
    xmm6 = F32(0, 0x009774a8);
    xmm1 = 0.0f;
    xmm2 = xmm6;
    xmm2 = phys_sub(xmm2, xmm0);
    cf = !(xmm2 >= xmm1);
    SU32(g, 0x1a0, 4);
    SF32(g, 0x1f8, xmm0);
    xmm0 = cf ? F32(0, 0x009cd9e8) : F32(0, 0x00931f24);   /* jb: -1.0 for a negative or unordered angle */
    xmm5 = F32(0, 0x00977498);
    SF32(L, 0xc, xmm0);
    xmm0 = phys_mul(xmm0, xmm2);
    xmm2 = xmm0;
    xmm2 = phys_mul(xmm2, F32(0, 0x009774a0));
    ecx = (int32_t)rx_cvtt(xmm2);
    xmm2 = (float)(int32_t)ecx;
    xmm2 = phys_mul(xmm2, F32(0, 0x0097749c));
    xmm0 = phys_sub(xmm0, xmm2);
    xmm5 = phys_sub(xmm5, xmm0);
    xmm2 = xmm5;
    xmm2 = phys_mul(xmm2, xmm5);
    xmm0 = xmm2;
    xmm0 = phys_mul(xmm0, F32(0, 0x00935d50));
    xmm0 = phys_add(xmm0, F32(0, 0x00935d48));
    xmm4 = xmm2;
    xmm4 = phys_mul(xmm4, xmm2);
    xmm0 = phys_mul(xmm0, xmm4);
    xmm3 = xmm2;
    xmm3 = phys_mul(xmm3, F32(0, 0x00935d44));
    xmm3 = phys_add(xmm3, F32(0, 0x00935d4c));
    xmm0 = phys_add(xmm0, xmm3);
    xmm0 = phys_mul(xmm0, xmm4);
    xmm3 = xmm2;
    xmm3 = phys_mul(xmm3, F32(0, 0x00935d40));
    xmm3 = phys_add(xmm3, F32(0, 0x00935d3c));
    xmm0 = phys_add(xmm0, xmm3);
    xmm3 = xmm6;
    xmm3 = phys_sub(xmm3, xmm0);
    xmm0 = xmm2;
    xmm0 = phys_mul(xmm0, F32(0, 0x00935d68));
    xmm0 = phys_add(xmm0, F32(0, 0x00935d64));
    xmm0 = phys_mul(xmm0, xmm4);
    xmm7 = xmm2;
    xmm7 = phys_mul(xmm7, F32(0, 0x00935d60));
    xmm7 = phys_add(xmm7, F32(0, 0x00935d5c));
    xmm2 = phys_mul(xmm2, F32(0, 0x00935d58));
    xmm2 = phys_add(xmm2, F32(0, 0x00935d54));
    xmm0 = phys_add(xmm0, xmm7);
    xmm7 = F32(g, 0x1b0);
    xmm0 = phys_mul(xmm0, xmm4);
    xmm0 = phys_add(xmm0, xmm2);
    xmm2 = F32(g, 0x1d0);
    xmm0 = phys_mul(xmm0, xmm5);
    xmm0 = phys_mul(xmm0, F32(L, 0xc));
    xmm5 = F32(g, 0x1e0);
    SF32(L, 0xc, xmm7);
    xmm6 = phys_sub(xmm6, xmm0);
    SF32(L, 0xd4, xmm6);
    xmm6 = F32(g, 0x1c0);
    xmm5 = phys_mul(xmm5, xmm1);
    SF32(L, 0x28, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x2c, xmm5);
    xmm4 = xmm2;
    xmm4 = phys_mul(xmm4, xmm1);
    xmm7 = xmm4;
    xmm7 = phys_add(xmm7, xmm6);
    xmm7 = phys_add(xmm7, xmm5);
    xmm5 = F32(g, 0x1e4);
    xmm7 = phys_add(xmm7, F32(L, 0xc));
    SF32(L, 0x68, xmm5);
    xmm5 = phys_mul(xmm5, xmm1);
    SF32(L, 0x1c, xmm5);
    xmm5 = F32(g, 0x1c4);
    SF32(L, 0x70, xmm7);
    xmm7 = F32(g, 0x1b4);
    SF32(L, 0x3c, xmm5);
    xmm5 = phys_mul(xmm5, xmm1);
    SF32(L, 0x34, xmm5);
    xmm5 = F32(g, 0x1d4);
    SF32(L, 0x58, xmm6);
    SF32(L, 0x10, xmm7);
    xmm6 = xmm5;
    xmm6 = phys_mul(xmm6, xmm1);
    xmm7 = phys_add(xmm7, xmm6);
    xmm7 = phys_add(xmm7, F32(L, 0x34));
    xmm7 = phys_add(xmm7, F32(L, 0x1c));
    SF32(L, 0x74, xmm7);
    xmm7 = F32(g, 0x1b8);
    SF32(L, 0x60, xmm6);
    xmm6 = F32(g, 0x1e8);
    SF32(L, 0x4c, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x20, xmm6);
    xmm6 = F32(g, 0x1c8);
    SF32(L, 0x40, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x24, xmm6);
    xmm6 = F32(g, 0x1d8);
    SF32(L, 0x30, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x54, xmm6);
    SF32(L, 0x14, xmm7);
    xmm7 = phys_add(xmm7, xmm6);
    xmm7 = phys_add(xmm7, F32(L, 0x24));
    xmm7 = phys_add(xmm7, F32(L, 0x20));
    xmm6 = F32(g, 0x1ec);
    SF32(L, 0x5c, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x18, xmm6);
    xmm6 = F32(g, 0x1cc);
    SF32(L, 0x6c, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x44, xmm6);
    xmm6 = F32(g, 0x1dc);
    SF32(L, 0x48, xmm6);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x64, xmm6);
    SF32(L, 0x78, xmm7);
    xmm7 = F32(g, 0x1bc);
    SF32(L, 0x50, xmm7);
    xmm7 = phys_add(xmm7, xmm6);
    xmm7 = phys_add(xmm7, F32(L, 0x44));
    xmm7 = phys_add(xmm7, F32(L, 0x18));
    xmm6 = F32(L, 0xc);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0xc, xmm6);
    SF32(L, 0x7c, xmm7);
    xmm7 = F32(L, 0x28);
    xmm7 = phys_mul(xmm7, xmm3);
    xmm6 = xmm2;
    xmm6 = phys_mul(xmm6, xmm0);
    xmm6 = phys_add(xmm6, xmm7);
    xmm6 = phys_add(xmm6, F32(L, 0x2c));
    xmm6 = phys_add(xmm6, F32(L, 0xc));
    xmm7 = F32(L, 0x3c);
    SF32(L, 0x80, xmm6);
    xmm6 = F32(L, 0x10);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x38, xmm6);
    xmm7 = phys_mul(xmm7, xmm3);
    xmm6 = xmm5;
    xmm6 = phys_mul(xmm6, xmm0);
    xmm6 = phys_add(xmm6, xmm7);
    xmm6 = phys_add(xmm6, F32(L, 0x38));
    xmm6 = phys_add(xmm6, F32(L, 0x1c));
    xmm7 = F32(L, 0x40);
    SF32(L, 0x84, xmm6);
    xmm6 = F32(L, 0x14);
    xmm6 = phys_mul(xmm6, xmm1);
    SF32(L, 0x14, xmm6);
    xmm6 = F32(L, 0x30);
    xmm6 = phys_mul(xmm6, xmm0);
    xmm7 = phys_mul(xmm7, xmm3);
    xmm6 = phys_add(xmm6, xmm7);
    xmm6 = phys_add(xmm6, F32(L, 0x14));
    xmm6 = phys_add(xmm6, F32(L, 0x20));
    xmm7 = F32(L, 0x6c);
    SF32(L, 0x88, xmm6);
    xmm6 = F32(L, 0x50);
    xmm6 = phys_mul(xmm6, xmm1);
    xmm1 = F32(L, 0x48);
    SF32(L, 0x10, xmm6);
    xmm6 = xmm1;
    xmm6 = phys_mul(xmm6, xmm0);
    xmm0 = xmm7;
    xmm0 = phys_mul(xmm0, xmm3);
    xmm6 = phys_add(xmm6, xmm0);
    xmm6 = phys_add(xmm6, F32(L, 0x10));
    xmm6 = phys_add(xmm6, F32(L, 0x18));
    SF32(L, 0x8c, xmm6);
    xmm6 = F32(L, 0xd4);
    xmm0 = F32(L, 0x28);
    xmm0 = phys_mul(xmm0, xmm6);
    xmm4 = phys_add(xmm4, F32(L, 0x58));
    xmm2 = phys_mul(xmm2, xmm3);
    xmm2 = phys_add(xmm2, xmm0);
    xmm2 = phys_add(xmm2, F32(L, 0x2c));
    xmm2 = phys_add(xmm2, F32(L, 0xc));
    xmm0 = F32(L, 0x3c);
    xmm0 = phys_mul(xmm0, xmm6);
    xmm5 = phys_mul(xmm5, xmm3);
    xmm5 = phys_add(xmm5, xmm0);
    xmm0 = F32(L, 0x30);
    xmm0 = phys_mul(xmm0, xmm3);
    SF32(L, 0x90, xmm2);
    xmm2 = F32(L, 0x38);
    xmm5 = phys_add(xmm5, xmm2);
    xmm5 = phys_add(xmm5, F32(L, 0x1c));
    SF32(L, 0x94, xmm5);
    xmm5 = F32(L, 0x40);
    xmm5 = phys_mul(xmm5, xmm6);
    xmm0 = phys_add(xmm0, xmm5);
    xmm5 = F32(L, 0x14);
    xmm0 = phys_add(xmm0, xmm5);
    xmm0 = phys_add(xmm0, F32(L, 0x20));
    SF32(L, 0x98, xmm0);
    xmm0 = F32(g, 0x1e0);
    xmm4 = phys_add(xmm4, xmm0);
    xmm0 = F32(L, 0x68);
    xmm4 = phys_add(xmm4, F32(L, 0xc));
    xmm0 = phys_add(xmm0, xmm2);
    xmm0 = phys_add(xmm0, F32(L, 0x60));
    xmm0 = phys_add(xmm0, F32(L, 0x34));
    SF32(L, 0xa4, xmm0);
    xmm0 = F32(L, 0x4c);
    xmm0 = phys_add(xmm0, xmm5);
    xmm0 = phys_add(xmm0, F32(L, 0x54));
    xmm0 = phys_add(xmm0, F32(L, 0x24));
    SF32(L, 0xa8, xmm0);
    xmm0 = F32(L, 0x5c);
    xmm1 = phys_mul(xmm1, xmm3);
    xmm3 = F32(L, 0x10);
    xmm7 = phys_mul(xmm7, xmm6);
    xmm1 = phys_add(xmm1, xmm7);
    xmm0 = phys_add(xmm0, xmm3);
    xmm0 = phys_add(xmm0, F32(L, 0x64));
    xmm0 = phys_add(xmm0, F32(L, 0x44));
    xmm1 = phys_add(xmm1, xmm3);
    xmm1 = phys_add(xmm1, F32(L, 0x18));
    SF32(L, 0x9c, xmm1);
    SF32(L, 0xa0, xmm4);
    SF32(L, 0xac, xmm0);
    for (uint32_t k = 0; k < 16; k++) SU32(g, 0x30 + 4 * k, U32(L, 0x70 + 4 * k));
    ACT_END(L);
}
RX_FUNCTION(0x006bdbe0, rx_drop_gate_reset)

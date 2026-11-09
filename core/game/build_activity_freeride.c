/* build_activity_freeride.c -- construction unit 11: MXActivityFreeRide, the front end's free ride activity (core/notes/race.md;
 * construction.md section 16). 0055fdb0 is its constructor: the Activity (005562c0), the activity's vtable (0x0094329c), the free
 * ride's jump tracker (+0xff080: 005857f0), the intro scenario (+0xff200, "FreerideIntroScenario", vtable 0x00944014), the
 * reset-marker flag entity (+0x1143e0: the scene unit's ModelEntity 00401db0 with the ResetMarkerFlagEntity vtables 0x00944054 /
 * +0x90 0x009440cc, enabled +0x70 = 1) and the FreeRideActivityAudioLogic (+0xfd8: ActivityAudioLogic with vtable 0x009429fc,
 * its vf32 00581030, then vtable 0x00942b1c).
 * The class's run-time methods are race_activity.c's (vf25 005604f0, vf26 005609b0, vf28 005609d0, vf31 00560290, vf42 00560b70);
 * the remaining ones (vf2 0055ff70, vf3 00560080, vf5 00560110, vf18 / vf19 / vf37 / vf44 / vf47 / vf52 / vf54) are other tasks. */
#include "build_activity_int.h"
#include "build_scene.h"

/* 005857f0 (EAX = the jump tracker, activity + 0xff080): its fields +0 .. +0x80 cleared */
void rx_free_ride_jump_tracker_init(rx_addr t) {
    SU32(t, 0, 0);
    for (uint32_t o = 4; o <= 0x80; o += 4) SU32(t, o, 0);
}
RX_FUNCTION(0x005857f0, rx_free_ride_jump_tracker_init)

/* 0055fdb0 (EAX = world, ECX = type; stdcall activity; RET 4) -> the activity */
rx_addr rx_free_ride_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(L, 0, 4);   /* push ecx (its slot unused at L+0xc); push ebx / esi / edi */
    ACT_CALL(1, rx_activity_construct(A, type, world));
    SU32(A, 0, 0x0094329c);                       /* MXActivityFreeRide::vftable */
    const rx_addr tr = A + 0xff080;               /* the jump tracker (005857f0; +0x84 / +0x100 / +0xb0 set before it) */
    SU32(tr, 0x84, 0);
    SU32(tr, 0x100, 0);
    SF32(tr, 0xb0, 0.0f);
    rx_free_ride_jump_tracker_init(tr);
    SU32(A, 0x1143a0, 0); SF32(A, 0x1143a4, 0.0f); SF32(A, 0x1143a8, 0.0f); SU32(A, 0x1143ac, 0);
    const rx_addr ent = A + 0x1143e0;             /* the reset-marker flag entity */
    SU32(A, 0xff214, 0);
    SU32(A, 0x1143b4, 0xf);
    SU32(A, 0xff200, 0x00944014);                 /* FreerideIntroScenario::vftable */
    ACT_CALL(1, rx_model_entity_construct(ent));  /* 00401db0 */
    SU32(ent, 0, 0x00944054);                     /* ResetMarkerFlagEntity::vftable */
    SU32(ent, 0x90, 0x009440cc);                  /* its IRenderable vtable */
    SU32(ent, 0x70, 1);
    SU32(A, 0xff214, A);
    /* the FreeRideActivityAudioLogic: ActivityAudioLogic (vtable 0x009429fc), vf32 00581030, the class's vtable 0x00942b1c */
    rx_addr l = rx_reflex_aligned_alloc(0x98);
    if (l) {
        SU32(l, 0, 0x009429fc);
        SU32(l, 0x94, 0);
        SU8(l, 4, 0);
        SU32(l, 0x50, 0); SU32(l, 0x54, 0); SU32(l, 0x8c, 0); SU32(l, 0x90, 0);
        rx_audio_logic_vf32(l);
        SU32(l, 0, 0x00942b1c);
    } else l = 0;
    SU32(A, 0xfd8, l);
    SU32(A, 0xff070, 0);
    SU32(A, 0x1143d0, 0);
    SU32(A, 0x114520, 0);
    SF32(A, 0xff074, 0.0f);
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0055fdb0, rx_free_ride_construct)

/* build_activity_freestyle.c -- construction of the Freestyle activities (the activity factory 0055ef80's types 5 / 0x14
 * (MXActivityFreestyle) and 0x20 / 0x3a / 0x3b / 0x3c (the Freestyle championship classes, built on the stub): the audio logic, the
 * stub (MXActivityFreestyleStub: the Activity, its three FMX scenarios and the audio logic) and the concrete class (the crowd
 * meter). The classes' virtual functions are in race_activity_freestyle.c. */
#include "build_activity_freestyle.h"
#include "build_activity_int.h"

/* 005623b0 (ESI = the logic) -> EAX: FreestyleActivityAudioLogic (0x98 bytes): the ActivityAudioLogic base (vtable 0x9429fc: +0x94,
 * the byte +4, +0x50 / +0x54 / +0x8c / +0x90 cleared; its cues reset 00581030, a direct call), then vtable 0x942bbc. Frame: push ecx. */
rx_addr rx_freestyle_audio_construct(rx_addr l) {
    ACT_FRAME(F, 0, 1);
    SU32(l, 0, 0x009429fc);
    SU32(l, 0x94, 0);
    SU8(l, 4, 0);
    SU32(l, 0x50, 0);
    SU32(l, 0x54, 0);
    SU32(l, 0x8c, 0);
    SU32(l, 0x90, 0);
    ACT_CALL(0, rx_audio_logic_vf32(l));
    SU32(l, 0, 0x00942bbc);
    ACT_END(F);
    return l;
}
RX_FUNCTION(0x005623b0, rx_freestyle_audio_construct)

/* 00562280 (ECX = type, EAX = world; stdcall activity; RET 4): MXActivityFreestyleStub: the Activity (005562c0), vtable 0x9441cc,
 * the three embedded scenarios (FMXIntroScenario +0xff080 vtable 0x94410c, FMXFinishScenario +0x114250 0x94414c, FMXResultsScenario
 * +0x129420 0x94418c; their states 0xf / 0 and owner pointers = the activity), +0x2c8 = 0, +0x22c = 1, a new audio logic (0x98
 * bytes) at +0xfd8, the Pre countdown +0x13e5f0 = 0. Frame: push ecx / ebx / esi / edi. */
rx_addr rx_freestyle_stub_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_construct(A, type, world));
    SU32(A, 0, 0x009441cc);
    SU32(A, 0x114224, 0);
    SU32(A, 0x114228, 0);
    SU32(A, 0x114220, 0);
    SU32(A, 0x11422c, 0);
    SU32(A, 0xff094, 0);
    SU32(A, 0xff080, 0x0094410c);
    SU32(A, 0x114234, 0xf);
    SU32(A, 0x1293f0, 0);
    SU32(A, 0x1293f4, 0);
    SU32(A, 0x1293f8, 0);
    SU32(A, 0x1293fc, 0);
    SU32(A, 0x114264, 0);
    SU32(A, 0x129404, 0xf);
    SU32(A, 0x114250, 0x0094414c);
    SU32(A, 0x13e5c0, 0);
    SU32(A, 0x13e5c4, 0);
    SU32(A, 0x13e5c8, 0);
    SU32(A, 0x13e5cc, 0);
    SU32(A, 0x129434, 0);
    SU32(A, 0x13e5d4, 0xf);
    SU32(A, 0x129420, 0x0094418c);
    SU32(A, 0x2c8, 0);
    SU32(A, 0x22c, 1);
    rx_addr l;
    ACT_CALL(1, l = rx_reflex_aligned_alloc(0x98));               /* cdecl: its argument is removed before the constructor */
    if (l) ACT_CALL(0, l = rx_freestyle_audio_construct(l));
    SU32(A, 0xfd8, l);
    SU32(A, 0x13e5f0, 0);
    SU32(A, 0xff094, A);
    SU32(A, 0x114264, A);
    SU32(A, 0x129434, A);
    ACT_END(F);
    return A;
}
RX_FUNCTION(0x00562280, rx_freestyle_stub_construct)

/* 00561910 (ESI = activity, ECX = type, EAX = world) -> EAX: MXActivityFreestyle (0x13e610 bytes): the stub, vtable 0x9442cc, +0x2c8
 * = 0, +0x22c = 1, the crowd meter: timer +0x13e600 = 0, peak +0x13e604 = 100, level +0x13e608 = 5. Frame: push ecx (the next push
 * is the stub's stack argument). */
rx_addr rx_freestyle_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_freestyle_stub_construct(A, type, world));
    SU32(A, 0, 0x009442cc);
    SU32(A, 0x2c8, 0);
    SU32(A, 0x22c, 1);
    SU32(A, 0x13e600, 0);
    SU32(A, 0x13e604, 100);
    SU32(A, 0x13e608, 5);
    ACT_END(F);
    return A;
}
RX_FUNCTION(0x00561910, rx_freestyle_construct)

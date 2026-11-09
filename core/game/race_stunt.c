/* race_stunt.c -- MXActivityRace's stunt event handlers: the race music state a rider's stunt starts or fails.
 *
 * Subscribed by MXActivityRace::vf2 00564b20 (build_activity_start.c rx_race_vf2) on the stunts channel [0x00a94304]:
 *   "StuntBegin"        00565770 (proxy 00588130, void(MXPlayer*, const StuntDescriptor*), RET 8)
 *   "StuntFailed"       005657d0 (proxy 00588130, void(MXPlayer*, const StuntDescriptor*), RET 8)
 *   "PhysicsStuntBegin" 00565710 (proxy 005881f0, void(MXPlayer*, const StuntDescriptor*, PhysicsStuntType), RET 0xc)
 * All three are called with ECX = the activity (MXActivityRace) and the player as the first stack argument. A non-local / AI
 * player (player +0x10 != 0) returns at once. A human sets the race music level (activity +0xff080) and its fade timer
 * (+0xff078) and the track music flag 0x009c3edc: the level unless the activity's audio logic (+0x234) mutes it (+0x22148 clear).
 * MXActivityRace::vf28 00565270 (rx_race_activity_in_progress) then fades the level from the activity's +0xff07c down to 5
 * over the timer's value, so a stunt begins the full-level fade (timer 4.0, level +0xff07c) and a failed one drops to 5 at once
 * (timer 0). The level written by 00565710 / 00565770 is +0xff07c itself; 005657d0 writes the constant 5.
 *
 * Gameplay: the level / flag are read by the race music and the crowd's state, and +0xff078 is read by vf28 every frame, so the
 * handlers run as ports (no boundary). */
#include "race.h"
#include "race_stunt.h"

/* 0x009c3edc = `level` unless the activity's audio logic (+0x234) mutes the track (+0x22148 clear): the original's
 * `mov edx,[edx+0x22148]; neg edx; sbb edx,edx; and edx,eax`. */
static void stunt_music_level(rx_addr act, uint32_t level) {
    const rx_addr audio = U32(act, 0x234);
    SU32(0, 0x009c3edc, audio != 0 ? (U32(audio, 0x22148) != 0 ? level : 0u) : level);
}

/* 00565710 (ECX = activity, args: player, stunt descriptor, type; RET 0xc): "PhysicsStuntBegin". A human: +0xff080 = +0xff07c
 * (the level the fade builds from), 0x009c3edc likewise (or 0 when muted), +0xff078 = 4.0 (0x9322c0, four seconds at 0.25/s). */
void rx_race_physics_stunt_begin(rx_addr act, rx_addr player, rx_addr desc, uint32_t type) {
    (void)desc;
    (void)type;
    if (U32(player, 0x10) != 0) return;
    const uint32_t level = U32(act, 0xff07c);
    SU32(act, 0xff080, level);
    stunt_music_level(act, level);
    SF32(act, 0xff078, 4.0f /* 0x9322c0 */);
}
RX_FUNCTION(0x00565710, rx_race_physics_stunt_begin)

/* 00565770 (ECX = activity, args: player, stunt descriptor; RET 8): "StuntBegin", the same body (two stack arguments). */
void rx_race_stunt_begin(rx_addr act, rx_addr player, rx_addr desc) {
    (void)desc;
    if (U32(player, 0x10) != 0) return;
    const uint32_t level = U32(act, 0xff07c);
    SU32(act, 0xff080, level);
    stunt_music_level(act, level);
    SF32(act, 0xff078, 4.0f /* 0x9322c0 */);
}
RX_FUNCTION(0x00565770, rx_race_stunt_begin)

/* 005657d0 (ECX = activity, args: player, stunt descriptor; RET 8): "StuntFailed": +0xff080 = 5, 0x009c3edc = 5 (or 0 when
 * muted) and the fade timer +0xff078 = 0.0 (xorps: the level drops to 5 at once). */
void rx_race_stunt_failed(rx_addr act, rx_addr player, rx_addr desc) {
    (void)desc;
    if (U32(player, 0x10) != 0) return;
    SU32(act, 0xff080, 5);
    stunt_music_level(act, 5);
    SF32(act, 0xff078, 0.0f);
}
RX_FUNCTION(0x005657d0, rx_race_stunt_failed)

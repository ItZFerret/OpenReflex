/* race_activity_audio.c -- ActivityAudioLogic (vtable 0x9429fc; one per activity, the activity at +0x50): the race's audio cues
 * and crowd reactions. It is an event listener of the activity: the activity's state changes and events ("Pre Exited", "Countdown
 * Step", "InProgress Entered", "PhysicsStuntEnd", ...) play the cues registered under that name (a std::multimap<string, Cue*> at
 * +0x8c), each optionally behind a modifier (DefaultModifier and its subclasses: conditions on the session state / activity kind,
 * "once", a countdown step, a human driver). Crowd reactions (FUN_00674440: raise the crowd excitement by an amount; FUN_006745d0:
 * a cheer / reaction of kind EAX) are gated by the GAME's random generator (0x9cb860), so these listeners are part of the game's
 * random sequence even though they only make sound: they are ported, not skipped.
 * Cue: +4 played, +8 sound name, +0xc modifier, +0x10 sound handle. Logic: +0x50 activity, +0x58 flag, +0x5c places of the
 * drivers (one per driver, the last standings index seen). */
#include "vehicle_audio.h"
#include "crt_atexit.h"
#include "race.h"
#include "crt.h"
#include "race_extern.h"
#include "career_activity.h"
#include "race_activity.h"
#include "world_wreck.h"
#include "build_stl_cue_map.h"
#include "build_series_init.h"
#include "fx.h"
#include "waypoint_audio.h"
#include "build_alloc.h"
#include "race_challenge_activity.h"
#include "sound_state.h"
#include "activity_cues.h"
#include "music_player.h"
#include "rider_cap.h"

/* Crowd excitement (FUN_00674440, cdecl float: the crowd level + amount, clamped) by a tuning constant (.data). */
static void crowd_excite(rx_addr amount) { const uint32_t v = U32(0, amount); RACE_CORE_CALL(1, rx_world_crowd_excite(rx_bf(v))); }

static void crowd_react(uint32_t kind) {   /* FUN_006745d0 (EAX = 1 / 2 / 3: which reaction plays on every crowd emitter) */
    RACE_CORE_CALL(0, rx_world_crowd_react(kind));
}

/* The inlined chance test: the game's LCG drawn in [0, 1) (bits 16..30 * 2^-15, exact) below the chance (.data). */
static int chance(rx_addr p) { return F32(0, p) > (float)race_rand() * 3.0517578125e-05f; }

/* An excitement then, by chance, the reaction: kind 1 for a negative excitement amount, else `kind`. */
static void crowd_cheer(rx_addr amount, rx_addr p, uint32_t kind) {
    crowd_excite(amount);
    if (chance(p)) crowd_react(0.0f > F32(0, amount) ? 1u : kind);
}

/* A cue modifier's vf2 (accept?) / vf3 (accepted) (ECX = modifier, args: name, cue, activity, a, b, 0, 0; RET 0x1c). */
typedef uint32_t (*modifier_fn)(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b);
static uint32_t modifier_call(rx_addr fn, rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b) {
    modifier_fn f = RX_FN(fn, modifier_fn);
    if (f) { uint32_t r; RACE_CORE_CALL(7, r = f(mod, name, cue, act, a, b)); return r; }
    uint32_t args[7] = {name, cue, act, a, b, 0, 0};
    return rx_call_original(fn, RX_THISCALL, mod, 7, args);
}

/* ---- cue dispatch ------------------------------------------------------------------------------------------------------------- */

/* 00585ad0 (EDX = cue map, EAX = key string) -> EAX: the number of cues under a name (equal_range FUN_00705f70, then the iterator
 * distance FUN_00709390 with EDI = the count). Frame: sub 0x18; push edi (the range at +8, the count at +4). */
uint32_t rx_audio_cue_count(rx_addr map, rx_addr key) {
    RACE_FRAME_PLAIN(F, 0x18, 1);
    RACE_CORE_CALL(3, (void)rx_cue_map_equal_range(map, F + 8, key));
    SU32(F + 4, 0, 0);
    RACE_CORE_CALL(4, rx_cue_range_count(F + 4, U32(F, 8), U32(F, 0xc), U32(F, 0x10), U32(F, 0x14)));
    const uint32_t n = U32(F, 4);
    RACE_FRAME_END(F);
    return n;
}
RX_FUNCTION(0x00585ad0, rx_audio_cue_count)

/* 005812f0 (ECX = logic, EDI = cue): plays a named cue (FUN_0067ead0 (ECX = 0, EDX = 1; cdecl name, the logic's name vf34, the
 * handle, 0)): marks it played and forgets the handle.
 * AUDIO BOUNDARY (0067ead0, the sound player: "play the sound of this name"): it resolves the name (0067e560 copies it into a local
 * buffer and expands "{...}" ids), finds or creates the audio event of that name and owner (0080c760: the audio event pool), gives
 * the event its channel record (0xd679a0 + 0x54 * channel), copies *handle (= cue + 0x10, read only) into the event's +0x128,
 * starts it (AudioEvent_Play: FMOD) and waits on a private kernel event. Everything it writes is the audio event pool and FMOD; the
 * one result (EBP, "played") is not used by this caller, and the cue's own fields (+4 played, +0x10 handle) are written below
 * whatever happened. The product has no sound engine, so it does not call it; the oracle runs the original. */
void rx_audio_play_cue(rx_addr logic, rx_addr cue) {
    RACE_FRAME_PLAIN(F, 8, 1);
    const rx_addr name = U32(cue, 8);
    if (U8(name, 0) != 0) {
        const uint32_t owner = race_thiscall(race_vfn(logic, 34), logic, 0, 0, 0, 0, 0);
#ifdef RX_ORACLE
        rx_regs in; memset(&in, 0, sizeof in); in.r[2] = 1;
        uint32_t a[4] = {name, owner, cue + 0x10, 0};
        rx_call_original_ex(0x0067ead0, &in, 0, 4, a);
#else
        (void)owner;
        rx_sound_cue_played(name, I32(cue, 0x10));   /* w-sfx: the cue for the host's activity sound (game/sound_state.c) */
#endif
        SU32(cue, 4, 1);
        SU32(cue, 0x10, 0xffffffffu);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005812f0, rx_audio_play_cue)

/* 005811a0 (stdcall logic, name, a, b; RET 0x10): plays the cues registered under `name`: every cue whose modifier (vf2 (name,
 * cue, activity, a, b, 0, 0)) accepts it, after the modifier's vf3 (same arguments); a cue without modifier always. The name is
 * copied into a std::string twice (count, then find: StringMap_Find with ESI = the map); the iterator walks with
 * TreeIterator_Increment (ESI = the iterator). Frame: and esp, -8; sub 0x2c; 3 pushes (the string at +0x1c, the iterator at
 * +0x14, the count at +0x10). */
void rx_audio_cue(rx_addr logic, rx_addr name, uint32_t a, uint32_t b) {
    RACE_FRAME_EBP(F, 8, 0x2c, 3);
    const rx_addr str = F + 0x1c, it = F + 0x14;
    if (U32(logic, 0x8c) != 0) {
        crt_str_ctor_cstr(str, name);   /* std::string(const char*) (IAT 0x9161ac) */
        uint32_t n;
        RACE_CORE_CALL(0, n = rx_audio_cue_count(U32(logic, 0x8c), str));
        SU32(F + 0x10, 0, n);
        crt_str_dtor(str);              /* ~string (IAT 0x9161b4) */
        crt_str_ctor_cstr(str, name);
        RACE_CORE_CALL(2, rx_world_string_map_find(U32(logic, 0x8c), it, str));   /* StringMap_Find (ESI = the map) */
        crt_str_dtor(str);
        const rx_addr head = U32(U32(logic, 0x8c), 0x18);
        if (U32(it, 4) != head) {
            for (; n != 0; n--) {
                const rx_addr cue = U32(U32(it, 4), 0x28);
                const rx_addr mod = U32(cue, 0xc);
                int play = 1;
                if (mod != 0) {
                    const rx_addr act = U32(logic, 0x50);
                    play = modifier_call(race_vfn(mod, 2), mod, name, cue, act, a, b) != 0;
                    if (play) modifier_call(race_vfn(mod, 3), mod, name, cue, act, a, b);
                }
                if (play) RACE_CORE_CALL(0, rx_audio_play_cue(logic, cue));
                RACE_CORE_CALL(0, rx_fx_tree_next(it));              /* TreeIterator_Increment (ESI = the iterator) */
            }
        }
#ifndef RX_ORACLE
        rx_activity_cues_play(logic, name, a, b);   /* w-sfx: the level's cues the product keeps outside guest memory (activity_cues.c) */
#endif
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005811a0, rx_audio_cue)

/* ---- RaceActivityAudioLogic's cue handlers (vf3 00582610 subscribes them for the cues the map holds; w-sfx) -------------------
 * Each plays the cues of its event name (005811a0) for the event's player (a driver's + 0x74, a vehicle's + 0x190). */
/* 00582d70 (ECX = logic; activity; RET 4): the "Restart" handler of vf29 00582ed0 (raised by Activity::vf16 00557b40, the restart of
 * the event: App.RestartActivity): the cues registered under "Restart" (005811a0), then the logic's vf32 (+0x80: 00582d50, the race's
 * cue state back to its start). Frame: push esi. */
void rx_race_audio_restart(rx_addr l, uint32_t act) {
    (void)act;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941ba0 /* "Restart" */, 0, 0));
    (void)race_thiscall(race_vfn(l, 32), l, 0, 0, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00582d70, rx_race_audio_restart)
/* 00582da0 (ECX = logic; the delegate's (activity, driver); RET 8): "EndOfRace" */
void rx_race_audio_end_of_race(rx_addr l, uint32_t act, uint32_t driver) {
    (void)act; (void)driver;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941c9c /* "EndOfRace" */, 0, 0));
}
RX_FUNCTION(0x00582da0, rx_race_audio_end_of_race)
/* 00582dc0 / 00582de0 / 00582e40 (ECX = logic; activity, driver; RET 8): "TwoLapsLeft" / "OneLapLeft" / "Holeshot" with the driver's player */
void rx_race_audio_two_laps(rx_addr l, uint32_t act, uint32_t driver) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941c74 /* "TwoLapsLeft" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00582dc0, rx_race_audio_two_laps)
void rx_race_audio_one_lap(rx_addr l, uint32_t act, uint32_t driver) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941c80 /* "OneLapLeft" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00582de0, rx_race_audio_one_lap)
void rx_race_audio_holeshot(rx_addr l, uint32_t act, uint32_t driver) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00942044 /* "Holeshot" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00582e40, rx_race_audio_holeshot)
/* 00582e00 (ECX = logic; driver, the lap time (float); RET 8): "Best Lap" */
void rx_race_audio_best_lap(rx_addr l, uint32_t driver, float time) {
    (void)time;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x0093ef0c /* "Best Lap" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00582e00, rx_race_audio_best_lap)
/* 00582e20 (ECX = logic; activity, vehicle; RET 8): "VehicleGettingLapped" with the vehicle's player (+0x190) */
void rx_race_audio_lapped(rx_addr l, uint32_t act, uint32_t vehicle) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941c5c /* "VehicleGettingLapped" */, U32(vehicle, 0x190), 0));
}
RX_FUNCTION(0x00582e20, rx_race_audio_lapped)
/* 00583fb0 (ECX = logic; activity, driver, two more; RET 0x10): "Player Wins": a tail jump into 005811a0 after rewriting its own four
 * argument slots (logic, name, the driver's player, 0), so the cue runs on the caller's frame */
void rx_race_audio_player_wins(rx_addr l, uint32_t act, uint32_t driver, uint32_t c, uint32_t d) {
    (void)act; (void)c; (void)d;
    const rx_addr sp = rx_sp();   /* [sp] = the return address */
    const uint32_t player = U32(driver, 0x74);
    SU32(sp, 4, l); SU32(sp, 8, 0x00941ca8 /* "Player Wins" */); SU32(sp, 0xc, player); SU32(sp, 0x10, 0);
    rx_audio_cue(l, 0x00941ca8, player, 0);
}
RX_FUNCTION(0x00583fb0, rx_race_audio_player_wins)
/* 00583a70 (ECX = logic; activity, driver, another driver; RET 0xc): "Player Eliminated" */
void rx_race_audio_eliminated(rx_addr l, uint32_t act, uint32_t driver, uint32_t other) {
    (void)act; (void)other;
    RACE_CORE_CALL(4, rx_audio_cue(l, 0x0093ef48 /* "Player Eliminated" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00583a70, rx_race_audio_eliminated)
/* 00582e60 (ECX = logic; activity, driver, n; RET 0xc): "TimedSectionEntered" when n > 1 */
void rx_race_audio_timed_section(rx_addr l, uint32_t act, uint32_t driver, int32_t n) {
    (void)act;
    if (n > 1) RACE_CORE_CALL(4, rx_audio_cue(l, 0x00941710 /* "TimedSectionEntered" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00582e60, rx_race_audio_timed_section)

/* ---- cue modifiers ------------------------------------------------------------------------------------------------------------ */

/* 00585a40 (EDI = the std::set<int>, args: out iterator, key*; RET 8) -> EAX = out: std::set<int>::find. The tree's lower bound
 * of *key (node +0xc = the key, +0 left, +8 right, +0x11 the nil flag; the set's head is [set + 0x18]); the result is the end
 * iterator (the head) when there is none or *key is below it. An iterator is { container proxy ([set + 0], checked: a null proxy
 * is _invalid_parameter_noinfo), node }. */
rx_addr rx_set_int_find(rx_addr set, rx_addr out, rx_addr key) {
    const rx_addr head = U32(set, 0x18);
    rx_addr lower = head, node = U32(head, 4);
    if (U8(node, 0x11) == 0) {
        const int32_t k = I32(key, 0);
        do {
            rx_addr next;
            if (I32(node, 0xc) < k) {
                next = U32(node, 8);
            } else {
                next = U32(node, 0);
                lower = node;
            }
            node = next;
        } while (U8(node, 0x11) == 0);
    }
    if (U32(set, 0) == 0) crt_invalid_parameter_noinfo();   /* the checked iterator of a set without a container proxy */
    if (lower == head || I32(key, 0) < I32(lower, 0xc)) {
        SU32(out, 0, U32(set, 0));
        SU32(out, 4, head);
    } else {
        SU32(out, 0, U32(set, 0));
        SU32(out, 4, lower);
    }
    return out;
}
RX_FUNCTION(0x00585a40, rx_set_int_find)

/* Is the key in the modifier's std::set<int> (00585a40, EDI = the set; stdcall out iterator, key*) -> != end. */
static int set_has(rx_addr set, rx_addr out, rx_addr key) {
    RACE_CORE_CALL(2, rx_set_int_find(set, out, key));
    return U32(out, 4) != U32(set, 0x18);
}

/* 00559da0 (ECX = the activity) -> EAX: 1 when one of its drivers (+0x268, count +0x26c) belongs to a local human player (the
 * player's +0x10 is 0). */
uint32_t rx_activity_has_local_human(rx_addr act) {
    const int32_t n = I32(act, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr driver = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (U32(U32(driver, 0x74), 0x10) == 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x00559da0, rx_activity_has_local_human)

/* 00580870 ActivityAudioLogic::DefaultModifier::vf2 (ECX = modifier, args: name, cue, activity, a, b, c, d; RET 0x1c) -> EAX: a
 * "once" modifier (+4) refuses a played cue; +8 requires FUN_00559da0 (ECX = activity); a non-empty set of session states (+0xc,
 * size +0x28) must hold the activity's (+0x238); a non-empty set of activity kinds (+0x2c, size +0x48) its kind (Activity_UiKind).
 * Frame: and esp, -8; sub 0x1c; 3 pushes (the iterators at +0x18 / +0x20, the kind at +0x10). */
uint32_t rx_audio_default_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b) {
    (void)name; (void)a; (void)b;
    RACE_FRAME_EBP(F, 8, 0x1c, 3);
    uint32_t ok = 0;
    if (U32(mod, 4) != 0 && U32(cue, 4) != 0) goto done;
    if (U32(mod, 8) != 0) {
        uint32_t human;
        RACE_CORE_CALL(0, human = rx_activity_has_local_human(act));   /* 00559da0 */
        if (human == 0) goto done;
    }
    if (U32(mod, 0x28) != 0 && !set_has(mod + 0xc, F + 0x18, act + 0x238)) goto done;
    if (U32(mod, 0x48) != 0) {
        uint32_t kind;
        RACE_CORE_CALL(0, kind = rx_activity_ui_kind(act));
        SU32(F + 0x10, 0, kind);
        if (!set_has(mod + 0x2c, F + 0x20, F + 0x10)) goto done;
    }
    ok = 1;
done:
    RACE_FRAME_END(F);
    return ok;
}
RX_FUNCTION(0x00580870, rx_audio_default_modifier)

/* 005809d0 ActivityAudioLogic::CountDownStepModifier::vf2 (same arguments) -> EAX: the default conditions and the countdown step
 * (*a, or -1 without) equal to the modifier's (+0x4c). */
uint32_t rx_audio_countdown_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr step, rx_addr b) {
    (void)b;
    uint32_t ok;
    RACE_CORE_CALL(7, ok = rx_audio_default_modifier(mod, name, cue, act, 0, 0));
    if (ok == 0) return 0;
    const int32_t s = step != 0 ? I32(step, 0) : -1;
    return s == I32(mod, 0x4c);
}
RX_FUNCTION(0x005809d0, rx_audio_countdown_modifier)

/* 00580a80 ActivityAudioLogic::HumanDriverModifier::vf2 (same arguments; ECX passed through) -> EAX: the default conditions and
 * `a` a human driver (a player +4 set, +0x10 == 0). */
uint32_t rx_audio_human_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr driver, rx_addr b) {
    (void)b;
    uint32_t ok;
    RACE_CORE_CALL(7, ok = rx_audio_default_modifier(mod, name, cue, act, 0, 0));
    return ok != 0 && U32(driver, 4) != 0 && U32(driver, 0x10) == 0;
}
RX_FUNCTION(0x00580a80, rx_audio_human_modifier)

/* ---- the listener's handlers -------------------------------------------------------------------------------------------------- */

/* 00581570 (stdcall logic; RET 4): place changes of the human drivers (a player's +0x10 == 0): the driver's standings index (the
 * activity's +0x2d0 table of 12, -1 if absent) replaces the one remembered (+0x5c + 4 * i); moving up into 3rd or 2nd: excitement
 * 0x9c47e8 and by chance 0x9c4810 a cheer (2); into the lead: excitement 0x9c47ec and by chance 0x9c4814 a cheer (2). */
void rx_audio_place_cues(rx_addr logic) {
    const rx_addr act = U32(logic, 0x50);
    const int32_t n = I32(act, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr ac = U32(logic, 0x50);
        const rx_addr d = U32(U32(ac, 0x268), 4 * i);
        if (U32(U32(d, 0x74), 0x10) != 0) continue;
        /* (w-rc-core) the remembered places of drivers 12..31 in the logic's side record (+0x5c holds 12) */
        const rx_addr mem = i < (int32_t)RX_RC_ORIG ? logic + 0x5c + 4 * (uint32_t)i : rx_rc_act(logic, 1) + 0x100 + 4 * ((uint32_t)i - RX_RC_ORIG);
        const int32_t was = I32(mem, 0);
        int32_t now = -1;
        for (uint32_t k = 0; k < RX_RC_MAX; k++) if (rx_stand(ac, k) == d) { now = (int32_t)k; break; }   /* (12 in the original) */
        SU32(mem, 0, (uint32_t)now);
        if ((was > 2 && now == 2) || (was > 1 && now == 1)) crowd_cheer(0x009c47e8, 0x009c4810, 2);
        else if (was > 0 && now == 0) crowd_cheer(0x009c47ec, 0x009c4814, 2);
    }
}
RX_FUNCTION(0x00581570, rx_audio_place_cues)

/* 00581010 ActivityAudioLogic::vf31 (ECX = logic, arg unused; RET 4): the per-frame update: place cues while enabled (vf1). */
void rx_audio_update(rx_addr logic) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (race_thiscall(race_vfn(logic, 1), logic, 0, 0, 0, 0, 0) != 0) RACE_CORE_CALL(1, rx_audio_place_cues(logic));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00581010, rx_audio_update)

/* 005818c0 ActivityAudioLogic::vf6 (ECX = logic, args: driver, x, y; RET 0xc): a human driver's event: excitement 0x9c47f0 and by
 * chance 0x9c4818 a cheer (2). */
void rx_audio_driver_cheer(rx_addr logic, rx_addr driver) {
    (void)logic;
    if (driver != 0 && U32(driver, 4) != 0 && U32(driver, 0x10) == 0) crowd_cheer(0x009c47f0, 0x009c4818, 2);
}
RX_FUNCTION(0x005818c0, rx_audio_driver_cheer)

/* 005819f0 ActivityAudioLogic::vf9 (ECX = logic, args: driver, x, y; RET 0xc): a stunt ended: the "PhysicsStuntEnd" cues (a =
 * the driver), then the human driver's cheer (as vf6). */
void rx_audio_stunt_end(rx_addr logic, rx_addr driver) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942794 /* "PhysicsStuntEnd" */, driver, 0));
    rx_audio_driver_cheer(logic, driver);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005819f0, rx_audio_stunt_end)

/* 005816e0: the countdown's last step: excitement 0x9c47dc and by chance 0x9c4804 the start reaction (3). */
void rx_audio_countdown_go(void) { crowd_cheer(0x009c47dc, 0x009c4804, 3); }
RX_FUNCTION(0x005816e0, rx_audio_countdown_go)

/* 00582150 ActivityAudioLogic::vf27 (ECX = logic, args: activity, step; RET 8): "Countdown Step": at step 0 while enabled (vf2)
 * the start reaction; then the "Countdown Step" cues with a = the address of the step (its argument slot). */
void rx_audio_countdown_step(rx_addr logic, rx_addr act, uint32_t step) {
    (void)act;
    const rx_addr slot = rx_sp() + 8;
    SU32(slot, 0, step);
    RACE_FRAME_PLAIN(F, 0, 1);
    if (race_thiscall(race_vfn(logic, 2), logic, 0, 0, 0, 0, 0) != 0 && step == 0) RACE_CORE_CALL(0, rx_audio_countdown_go());
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00941bdc /* "Countdown Step" */, slot, 0));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00582150, rx_audio_countdown_step)

/* The music (FUN_00677700: EAX = the music player 0xa946b8 +0x15c6c, ESI = the activity kind) for the race in progress.
 * AUDIO BOUNDARY (the music player's "start the playlist of this activity kind"): when the player is not already stopped (+8 not 0 /
 * 2) it resets itself (00677510: stops its three FMOD event slots +0x18 / +0x1e0 / +0x3a0 through 0080ae10 or the audio command
 * queue 0080c280, state words +0xc / +0x10 / +0x14 = 4, +8 = 1, +4 = 0, +0x1d8 = 0, the fade globals 0xd67ab8..), then with the
 * kind's playlist (the table 0xc01c10 / 0xc01fdc) selects it (+0x1d8 the list, +0x1dc the kind, +4 = 1, +8 = 2) and picks the
 * first track (006771b0: +0x570 / +0x1d0). Every field it writes belongs to the music player object, which only the music
 * functions (00677xxx) and the FMOD event slots use; no gameplay code reads it. The product has no sound engine and does not
 * create the music player, so it does not call it; the oracle runs the original. */
static void music_for_kind(uint32_t kind) {
#ifdef RX_ORACLE
    rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(0, 0xa946b8) + 0x15c6c; in.r[6] = kind;
    rx_call_original_ex(0x00677700, &in, 0, 0, 0);
#else
    rx_music_jukebox(kind);   /* w-sfx: the player's state (game/music_player.c) */
#endif
}

/* 00582270 (ECX = logic, arg: activity; RET 8; shared slot 19 "InProgress Entered"): the cues, +0x58 cleared, the music of the
 * activity kind (ESI; series events from 0x1f: the series manager's kind, behind the static guard 0x9e275c). */
void rx_audio_in_progress(rx_addr logic, rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427e4 /* "InProgress Entered" */, 0, 0));
    SU32(logic, 0x58, 0);
    uint32_t kind = U32(act, 0x23c);
    if (kind >= 0x1f) {
        if ((U8(0, 0x9e275c) & 1) == 0) {
            SU32(0, 0x9e275c, U32(0, 0x9e275c) | 1);
            RACE_CORE_CALL(0, (void)rx_series_manager_construct());   /* MXSeriesManager_Construct */
            RX_ATEXIT(0x00911da0);      /* _atexit */
        }
        RACE_CORE_CALL(1, kind = rx_career_current_event_type(0x009e2188));   /* 005b6a30 (stdcall manager; RET 4) */
    }
    music_for_kind(kind);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00582270, rx_audio_in_progress)

/* 005822e0 FreeRideActivityAudioLogic::vf19 (ECX = logic, arg: activity; RET 8): "InProgress Entered" cues, +0x58 cleared, and
 * unless the audio settings say otherwise (+0x234 +0x20018) the music of the activity kind. */
void rx_audio_free_ride_in_progress(rx_addr logic, rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427e4, 0, 0));
    SU32(logic, 0x58, 0);
    if (U32(U32(act, 0x234), 0x20018) == 0) {
        uint32_t kind;
        RACE_CORE_CALL(0, kind = rx_activity_ui_kind(act));
        music_for_kind(kind);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005822e0, rx_audio_free_ride_in_progress)

/* 00581f90 ActivityAudioLogic::vf16 (ECX = logic; RET 8): "Pre Exited" cues, FUN_006818e0 (EAX = 0xd92f00; 1), and with the
 * music player in state 4 its fade out. */
void rx_audio_pre_exited(rx_addr logic) {
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427b0 /* "Pre Exited" */, 0, 0));
#ifndef RX_ORACLE
    rx_vehicle_audio_pre_exited();   /* w-audio: 006818e0(1) as a cue for the host's engine emitters (product only, reads nothing) */
    if (U32(U32(0, 0xa946b8), 0x15c74) == 4) rx_music_fade_out();   /* w-sfx: MusicPlayer_FadeOut on the player's state (game/music_player.c) */
#else   /* PRODUCT BOUNDARY (core/plan/QUEUE.md "chain"): the ambient / music players (006818e0, MusicPlayer_FadeOut 00677600) drive the sound engine only */
    rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(0, 0xd92f00);
    uint32_t one = 1;
    rx_call_original_ex(0x006818e0, &in, 0, 1, &one);
    const rx_addr mp = U32(0, 0xa946b8);
    if (U32(mp, 0x15c74) == 4) {
        memset(&in, 0, sizeof in); in.r[0] = mp + 0x15c6c;
        rx_call_original_ex(0x00677600, &in, 0, 0, 0);                           /* MusicPlayer_FadeOut */
    }
#endif
}
RX_FUNCTION(0x00581f90, rx_audio_pre_exited)

/* 00581fd0 vf17 / 00581ff0 vf18 (ECX = logic; RET 8): the "Countdown Entered" / "Countdown Exited" cues. */
void rx_audio_countdown_entered(rx_addr logic) { RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427bc /* "Countdown Entered" */, 0, 0)); }
RX_FUNCTION(0x00581fd0, rx_audio_countdown_entered)
void rx_audio_countdown_exited(rx_addr logic) { RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427d0 /* "Countdown Exited" */, 0, 0)); }
RX_FUNCTION(0x00581ff0, rx_audio_countdown_exited)

/* 00581760 (ESI = logic): the end of race's crowd cheer, once per logic (+0x58): the crowd excitement by the EndRace level
 * (0x9c47e0, 00674440) and, by the EndRace chance (0x9c4808, one draw of the game's LCG), the reaction (3, or 1 for a negative level).
 * (w-sfx: this was a product boundary named "picks and starts the race music"; it is the crowd's, and its draw is the game's.) */
void rx_audio_end_race_cheer(rx_addr logic) {
    if (U32(logic, 0x58) != 0) return;
    crowd_cheer(0x009c47e0, 0x009c4808, 3);
    SU32(logic, 0x58, 1);
}
RX_FUNCTION(0x00581760, rx_audio_end_race_cheer)
static void music_start(rx_addr logic) { RACE_CORE_CALL(0, rx_audio_end_race_cheer(logic)); }
/* 00677510 stops the music player's three events (+0x18 / +0x1e0 / +0x3a0): the sound engine's; the product's music player state is
 * game/music_player.c's (the oracle runs the original). */
static void music_stop_all(void) {
#ifdef RX_ORACLE
    rx_regs in; memset(&in, 0, sizeof in); in.r[7] = U32(0, 0xa946b8) + 0x15c6c;   /* EDI = the music player */
    rx_call_original_ex(0x00677510, &in, 0, 0, 0);
#else
    rx_music_stop();   /* w-sfx (game/music_player.c) */
#endif
}

/* 00583130 ActivityAudioLogic::vf20 (ECX = logic, args: activity, new state; RET 8): the "InProgress Exited" cues; with the new
 * state 0, 1 or 5 the music player stopped (00677510). */
void rx_audio_in_progress_exited(rx_addr logic, uint32_t act, uint32_t state) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427f8 /* "InProgress Exited" */, 0, 0));
    if ((int32_t)state < 0 || (state > 1 && state != 5)) return;
    music_stop_all();
}
RX_FUNCTION(0x00583130, rx_audio_in_progress_exited)

/* 00583170 ActivityAudioLogic::vf21 (ECX = logic; RET 8): the "WindDown Entered" cues and the race music (00581760). */
void rx_audio_wind_down_entered(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x0094280c /* "WindDown Entered" */, 0, 0));
    music_start(logic);
}
RX_FUNCTION(0x00583170, rx_audio_wind_down_entered)

/* 00582070 ActivityAudioLogic::vf22 (ECX = logic; RET 8): the "WindDown Exited" cues. */
void rx_audio_wind_down_exited(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942820 /* "WindDown Exited" */, 0, 0));
}
RX_FUNCTION(0x00582070, rx_audio_wind_down_exited)

/* 005848a0 ActivityAudioLogic::vf23 (ECX = logic; RET 8): the "Post Entered" cues; with the logic's +0x58 clear and its vf2 set the
 * race music (00581760), the logic's vf37 (00582220), then the music player stopped (00677510). Frame: push esi, edi. */
void rx_audio_post_entered(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942830 /* "Post Entered" */, 0, 0));
    if (U32(logic, 0x58) == 0 && race_thiscall(race_vfn(logic, 2), logic, 0, 0, 0, 0, 0) != 0) music_start(logic);
    race_thiscall(race_vfn(logic, 37), logic, 0, 0, 0, 0, 0);
    music_stop_all();
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005848a0, rx_audio_post_entered)

/* 005820d0 ActivityAudioLogic::vf24 (ECX = logic; RET 8): the "Post Exited" cues and, with the music player in state 4, its fade
 * out (00677600). */
void rx_audio_post_exited(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942840 /* "Post Exited" */, 0, 0));
#ifdef RX_ORACLE   /* MusicPlayer_FadeOut (00677600): the original; the product runs its state port (game/music_player.c, w-sfx) */
    const rx_addr mp = U32(0, 0xa946b8);
    if (U32(mp, 0x15c74) == 4) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = mp + 0x15c6c;
        rx_call_original_ex(0x00677600, &in, 0, 0, 0);
    }
#else
    if (U32(U32(0, 0xa946b8), 0x15c74) == 4) rx_music_fade_out();
#endif
}
RX_FUNCTION(0x005820d0, rx_audio_post_exited)

/* ---- the base ActivityAudioLogic state cues the challenges' logic (ChallengeActivityAudioLogic, vtable 0x009431fc) keeps ---- */

/* 00581ba0 ActivityAudioLogic::vf10 (ECX = logic, args: activity, vehicle; RET 8): the "VehicleWrecked" cues with the vehicle's
 * +0x190 */
void rx_audio_logic_vehicle_wrecked(rx_addr logic, uint32_t act, rx_addr vehicle) {
    (void)act;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x0093ee38 /* "VehicleWrecked" */, U32(vehicle, 0x190), 0));
}
RX_FUNCTION(0x00581ba0, rx_audio_logic_vehicle_wrecked)

/* 00582030 ActivityAudioLogic::vf20 (ECX = logic; RET 8): the "InProgress Exited" cues */
void rx_audio_logic_in_progress_exited(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009427f8 /* "InProgress Exited" */, 0, 0));
}
RX_FUNCTION(0x00582030, rx_audio_logic_in_progress_exited)

/* 00582050 ActivityAudioLogic::vf21 (ECX = logic; RET 8): the "WindDown Entered" cues */
void rx_audio_logic_wind_down_entered(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x0094280c /* "WindDown Entered" */, 0, 0));
}
RX_FUNCTION(0x00582050, rx_audio_logic_wind_down_entered)

/* 00582090 ActivityAudioLogic::vf23 (ECX = logic; RET 8): the "Post Entered" cues; with the logic's +0x58 clear and its vf2 set the
 * race music (00581760), then the logic's vf37 (the music player is not stopped: that is RaceActivityAudioLogic's 005848a0).
 * Frame: push esi. */
void rx_audio_logic_post_entered(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942830 /* "Post Entered" */, 0, 0));
    if (U32(logic, 0x58) == 0 && race_thiscall(race_vfn(logic, 2), logic, 0, 0, 0, 0, 0) != 0) music_start(logic);
    race_thiscall(race_vfn(logic, 37), logic, 0, 0, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00582090, rx_audio_logic_post_entered)

/* 00582180 ActivityAudioLogic::vf28 (ECX = logic, args: activity, step; RET 8): the "FinishCountdown Step" cues with the address of
 * the step argument */
void rx_audio_logic_finish_countdown_step(rx_addr logic, uint32_t act, uint32_t step) {
    (void)act; (void)step;
    const rx_addr step_slot = rx_sp() + 8;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00941bec /* "FinishCountdown Step" */, step_slot, 0));
}
RX_FUNCTION(0x00582180, rx_audio_logic_finish_countdown_step)

/* ---- FreeRideActivityAudioLogic's cue handler (subscribed by its vf3 00582330, build_activity_audio.c) ---- */

/* FreeRideActivityAudioLogic::vf38 00582430 (ECX = logic; the "ChallengeAwardAchieved" payload: player, kind, medal; RET 0xc),
 * reached through the vcall thunk 00592bb0 (slot 0x98): the "ChallengeAwardAchieved" cues with the player and the address of the
 * medal argument (`mov edx, [esp+4]; lea eax, [esp+0xc]; push eax; push edx; push 0x942870; push ecx; call 005811a0; ret 0xc`).
 * The event is raised by 0054e670 (race_challenge.c) for every medal of a Free Ride award: the discoveries (00585330: the medal at a
 * third / two thirds / all of the level's discoveries, so the third of FR_Cheyenne's nine) and the jump distances (005851f0).
 * Unported, the product stopped there ("unported game function 00582430", playtester report 2026-11-08, FR_Cheyenne). */
void rx_free_ride_audio_award_achieved(rx_addr logic, rx_addr player, uint32_t kind, uint32_t medal) {
    (void)kind; (void)medal;
    const rx_addr medal_slot = rx_sp() + 0xc;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942870 /* "ChallengeAwardAchieved" */, player, medal_slot));
}
RX_FUNCTION(0x00582430, rx_free_ride_audio_award_achieved)

/* ---- ChallengeActivityAudioLogic's cue handlers (subscribed by its vf3 00584ed0, build_activity_audio.c) ---- */

/* 00585150 (ECX = logic; the "ChallengeAward" payload: activity, driver, medal 2, medal 1, medal 0; RET 0x14): the best medal won
 * (2 / 1 / 0, written into the medal 2 slot) and the "ChallengeAward" cues with the driver's player and that slot's address */
void rx_challenge_audio_award(rx_addr logic, rx_addr act, rx_addr driver, uint32_t m2, uint32_t m1, uint32_t m0) {
    (void)act;
    const rx_addr slot = rx_sp() + 0xc;
    uint32_t level;
    if (m2 != 0) level = 2;
    else if (m1 != 0) level = 1;
    else if (m0 != 0) level = 0;
    else return;
    SU32(slot, 0, level);
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x009424f4 /* "ChallengeAward" */, U32(driver, 0x74), slot));
}
RX_FUNCTION(0x00585150, rx_challenge_audio_award)

/* 005851a0 (ECX = logic; the "TargetPracticeFound" payload: player, hits, total, position; RET 0x10): a tail jump to the cue player
 * (005811a0: logic, "TargetPracticeFound", player, 0) through the four argument slots */
void rx_challenge_audio_target_found(rx_addr logic, rx_addr player, uint32_t hits, uint32_t total, rx_addr pos) {
    (void)hits; (void)total; (void)pos;
    const rx_addr entry = rx_sp();
    SU32(entry, 0x10, 0);
    SU32(entry, 0xc, player);
    SU32(entry, 8, 0x009419b0);
    SU32(entry, 4, logic);
    rx_audio_cue(logic, 0x009419b0 /* "TargetPracticeFound" */, player, 0);
}
RX_FUNCTION(0x005851a0, rx_challenge_audio_target_found)

/* 005851d0 (ECX = logic; the "HitDiscoveryFound" payload (three words); RET 0xc): the "HitDiscoveryFound" cues with the first word */
void rx_challenge_audio_hit_discovery(rx_addr logic, uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x00942898 /* "HitDiscoveryFound" */, a, 0));
}
RX_FUNCTION(0x005851d0, rx_challenge_audio_hit_discovery)

/* 00584e90 ChallengeActivityAudioLogic::vf38 (the scalar deleting destructor; ECX = logic; flags; RET 4): the vtable, the music player
 * stopped (00677510, EDI = [0xa946b8] + 0x15c6c: PRODUCT BOUNDARY, see music_stop_all), the logic destroyed (00580d30), freed with
 * flag 1. Frame: push esi; push edi. */
rx_addr rx_challenge_audio_logic_sdtor(rx_addr logic, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 2);
    SU32(logic, 0, 0x009431fc);
    music_stop_all();
    RACE_CORE_CALL(1, rx_activity_audio_destroy(logic));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(logic));
    RACE_FRAME_END(F);
    return logic;
}
RX_FUNCTION(0x00584e90, rx_challenge_audio_logic_sdtor)

/* 00581bc0 ActivityAudioLogic::vf12 (ECX = logic, args: activity, driver; RET 8): a crash of a human driver of this activity in
 * one of the bail states (+0x190 player +0xb44 in 2, 3, 5, 6, 8, 9, 11, 12): FUN_006817c0 (1). */
void rx_audio_crash(rx_addr logic, rx_addr act, rx_addr driver) {
    if (act != U32(logic, 0x50) || driver == 0) return;
    const rx_addr p = U32(driver, 0x190);
    if (p == 0 || U32(p, 4) == 0 || U32(p, 0x10) != 0) return;
    const uint32_t s = U32(p, 0xb44) - 2;
    if (s <= 10 && ((0x6dbu >> s) & 1)) {
        RACE_CORE_CALL(1, rx_world_rider_sound_start(driver, 1));               /* ESI = the driver */
    }
}
RX_FUNCTION(0x00581bc0, rx_audio_crash)

/* 00555df0 ActivityAudioLogic::vf34 -> EAX: the listener's name "ActivityAudioLogic" (0x9419f4). */
rx_addr rx_audio_name(void) { return 0x009419f4; }
RX_FUNCTION(0x00555df0, rx_audio_name)

/* 00555de0 ActivityAudioLogic::DefaultModifier::vf3 (RET 0x1c): nothing. */
uint32_t rx_audio_modifier_nop(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b) {
    (void)mod; (void)name; (void)cue; (void)act; (void)a; (void)b;
    return 0;
}
RX_FUNCTION(0x00555de0, rx_audio_modifier_nop)

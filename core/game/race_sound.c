/* race_sound.c -- the sound calls of the race layer's rules (the cannonball tunnel's warning loop and the music fade): the event
 * stop / volume wrappers that run on the audio thread or queue a command for it, the music channel volume, and the name the
 * warning event is created under. Event layout and the queue: world_node_audio.c (core/notes/race.md "Rules"). The FMOD calls
 * (FMOD::Event::stop, Event::setVolume through 0080b490) stay original (the audio DLL is a boundary). */
#include "race.h"
#include "race_extern.h"
#include "world_node.h"
#include "loop_platform.h"

/* the audio log called with its arguments on the game stack (it reads the varargs from there) */
static void audio_log_args(int n, const uint32_t* a) {
    const rx_addr sp = rx_sp();
    for (int i = 0; i < n; i++) SU32(sp - 4u * (uint32_t)(n - i), 0, a[i]);
    rx_set_sp(sp - 4u * (uint32_t)n - 4u);
    rx_world_audio_log(a[0], a[1]);
    rx_set_sp(sp);
}

/* ---- the FMOD calls (the audio DLL is a boundary) ------------------------------------------------------------------------------
 * FMOD_EVENTL.DLL is the audio engine: the oracle runs the emulator's stand-in through the import thunk, the product has no audio
 * engine and the calls report FMOD_OK (0). What the game does with the results: they go to the audio log (rx_world_audio_log: a
 * debug text in a stack buffer, nothing else) and the setVolume result 0x24 (FMOD_ERR_INVALID_HANDLE) triggers
 * rx_audio_event_invalid_handle; the event record's own fields (state, handle) are written by the callers whatever the result. So
 * nothing the game reads depends on the engine's answers. */
uint32_t rx_fmod_event_stop(rx_addr fmod_event, uint32_t immediate) {         /* 00900378 FMOD::Event::stop (stdcall this, immediate) */
#ifdef RX_ORACLE
    const uint32_t a[2] = {fmod_event, immediate};
    return rx_call_original(0x00900378, RX_STDCALL, 0, 2, a);
#else
    (void)fmod_event; (void)immediate;
    return 0;
#endif
}
uint32_t rx_fmod_event_set_volume(rx_addr fmod_event, float volume) {        /* 00900342 FMOD::Event::setVolume (stdcall this, volume) */
#ifdef RX_ORACLE
    const uint32_t a[2] = {fmod_event, rx_fb(volume)};
    return rx_call_original(0x00900342, RX_STDCALL, 0, 2, a);
#else
    (void)fmod_event; (void)volume;
    return 0;
#endif
}
uint32_t rx_fmod_event_set_callback(rx_addr fmod_event, uint32_t callback, uint32_t user) {   /* 00900366 FMOD::Event::setCallback (stdcall this, callback, user) */
#ifdef RX_ORACLE
    const uint32_t a[3] = {fmod_event, callback, user};
    return rx_call_original(0x00900366, RX_STDCALL, 0, 3, a);
#else
    (void)fmod_event; (void)callback; (void)user;
    return 0;
#endif
}

/* 0080ae10 (ESI = event; EDI saved; no result): stops the event's FMOD instance now (we are on the audio thread): not while it is
 * in state 2; FMOD::Event::stop (event, 0), the result through the audio log ("FMODEventID %u - FMOD::Event::stop() on %s"), and
 * unless the event's flag +0x13c bit 3 is set an idle state (0 / 3 / 5 ...) becomes 5 (stopped). Frame: push edi. */
void rx_sound_stop_now(rx_addr ev) {
    const rx_addr fe = U32(ev, 0x118);
    if (fe == 0) return;
    if (U32(ev, 0x138) == 2) return;
    RACE_FRAME_PLAIN(F, 0, 1);
    const uint32_t keep = (U8(ev, 0x13c) & 8) != 0;
    uint32_t r;
    RACE_CORE_CALL(2, r = rx_fmod_event_stop(fe, 0));                        /* FMOD::Event::stop */
    const uint32_t l[4] = {r, 0x009733e8, U32(ev, 0x14), ev + 0x18};
    audio_log_args(4, l);
    RACE_FRAME_END(F);
    if (keep) return;
    const uint32_t s = U32(ev, 0x138);
    if (s == 2 || s == 1) return;
    SU32(ev, 0x138, 5);
}
RX_FUNCTION(0x0080ae10, rx_sound_stop_now)

/* 0080ade0 (EAX = event, EBP = the caller's, passed on to the queue; no result): stops a sound event: on the audio thread
 * ([0xd67d1c]) now (0080ae10), else the queued command 1 (0080c280). Frame: push esi, push edi. */
void rx_sound_stop(rx_addr ev, uint32_t ebp) {
    RACE_FRAME_PLAIN(F, 0, 2);
    if (U32(0, 0xd67d1c) == loop_get_current_thread_id()) {
        RACE_CORE_CALL(0, rx_sound_stop_now(ev));
    } else {
        RACE_CORE_CALL(4, rx_world_event_queue(1, ev, 0, 0, ebp));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0080ade0, rx_sound_stop)

/* 0080b410 (ESI = event, xmm0 = volume, EBP = the caller's; no result): the event's volume clamped to 0..1 (+0x11c; a NaN stays),
 * applied on the audio thread (a tail call of 0080b490, EAX = ESI, original: FMOD::Event::setVolume) or queued (command 2).
 * Frame: push edi around the thread check. */
void rx_sound_set_volume(rx_addr ev, float v, uint32_t ebp) {
    if (v >= 1.0f /* 0x931f24 */) v = 1.0f;
    else if (0.0f >= v) v = 0.0f;
    SF32(ev, 0x11c, v);
    if (U32(0, 0xd67d1c) == loop_get_current_thread_id()) {
        rx_audio_event_apply_volume(ev);                 /* AudioEvent_ApplyVolume 0080b490 (a tail jump: EAX = ESI = event) */
    } else {
        RACE_CORE_CALL(4, rx_world_event_queue(2, ev, 0, 0, ebp));
    }
}
RX_FUNCTION(0x0080b410, rx_sound_set_volume)

/* 0080b490 AudioEvent_ApplyVolume (EAX = ESI = the event): the event's FMOD instance (+0x118) given its volume: the product of the
 * channel record's three factors (+4, +0, +0x18 of the event's channel +0x1a0, the global record 0xd679a0 without one) and the
 * event's own volume (+0x11c); FMOD::Event::setVolume, its result through the audio log ("FMODEventID %u - FMOD::Event::setVolume()
 * on %s"), and 0x24 (an invalid handle) sends the event to 0080a8e0. Frame: push esi (the volume is built in the second pushed
 * argument slot of the call). */
void rx_audio_event_apply_volume(rx_addr ev) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const rx_addr fe = U32(ev, 0x118);
    if (fe != 0) {
        const rx_addr channel = U32(ev, 0x1a0) != 0 ? U32(ev, 0x1a0) : 0x00d679a0;
        float v = rx_muls(F32(channel, 4), F32(channel, 0));
        v = rx_muls(v, F32(channel, 0x18));
        v = rx_muls(v, F32(ev, 0x11c));
        uint32_t r;
        RACE_CORE_CALL(2, r = rx_fmod_event_set_volume(fe, v));
        const uint32_t l[4] = {r, 0x009737bc, U32(ev, 0x14), ev + 0x18};
        audio_log_args(4, l);
        if (r == 0x24) RACE_CORE_CALL(0, rx_audio_event_invalid_handle(ev));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0080b490, rx_audio_event_apply_volume)

/* 0080dcd0 (ECX = &the event): the event taken out of the audio event list (data [0xd67d04], count [0xd67d08]): the first equal
 * entry replaced by the last. */
void rx_audio_event_unlist_entry(rx_addr event_slot) {
    const int32_t n = I32(0, 0x00d67d08);
    if (n <= 0) return;
    const rx_addr data = U32(0, 0x00d67d04);
    const uint32_t ev = U32(event_slot, 0);
    for (int32_t i = 0; i < n; i++) {
        if (U32(data, 4 * (uint32_t)i) == ev) {
            SU32(data, 4 * (uint32_t)i, U32(data, 4 * (uint32_t)(n - 1)));
            SU32(0, 0x00d67d08, U32(0, 0x00d67d08) - 1);
            return;
        }
    }
}
RX_FUNCTION(0x0080dcd0, rx_audio_event_unlist_entry)

/* 0080bde0 (cdecl event; the caller cleans): under the event system's lock 0xd67fb8: a listed event (+0x144 bit 0) leaves the list
 * (0080dcd0, ECX = &the argument slot), its flags +0x144 lose bit 0 and bits 1..4 (& 0xffffffe1), and +0x13c / +0x140 are cleared. */
void rx_audio_event_reset_flags(rx_addr ev) {
    const rx_addr arg_slot = rx_sp() + 4;
    SU32(arg_slot, 0, ev);          /* the caller's pushed argument (0080dcd0 reads it through ECX = its address) */
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, loop_enter_cs(0x00d67fb8));
    if ((U8(ev, 0x144) & 1) != 0) {
        RACE_CORE_CALL(0, rx_audio_event_unlist_entry(arg_slot));
        SU32(ev, 0x144, U32(ev, 0x144) & 0xfffffffeu);
    }
    SU32(ev, 0x144, U32(ev, 0x144) & 0xffffffe1u);
    SU32(ev, 0x13c, 0);
    SU32(ev, 0x140, 0);
    RACE_CORE_CALL(1, loop_leave_cs(0x00d67fb8));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0080bde0, rx_audio_event_reset_flags)

/* 0080a8e0 (ESI = event): the event's FMOD instance (+0x118) reported invalid: its callback cleared (FMOD::Event::setCallback (event,
 * 0, 0), the result through the audio log) and the event's flags reset (0080bde0); the instance dropped (+0x118 = 0), an idle state
 * (not 1 / 2) back to 0, and the two ids (+0x124, +0x128) to -1. */
void rx_audio_event_invalid_handle(rx_addr ev) {
    const rx_addr fe = U32(ev, 0x118);
    if (fe != 0) {
        RACE_FRAME_PLAIN(F, 0, 0);
        uint32_t r;
        RACE_CORE_CALL(3, r = rx_fmod_event_set_callback(fe, 0, 0));
        const uint32_t l[4] = {r, 0x00973414, U32(ev, 0x14), ev + 0x18};
        audio_log_args(4, l);
        RACE_CORE_CALL(1, rx_audio_event_reset_flags(ev));
        RACE_FRAME_END(F);
    }
    const uint32_t state = U32(ev, 0x138);
    SU32(ev, 0x118, 0);
    if (state != 2 && state != 1) SU32(ev, 0x138, 0);
    SU32(ev, 0x124, 0xffffffffu);
    SU32(ev, 0x128, 0xffffffffu);
}
RX_FUNCTION(0x0080a8e0, rx_audio_event_invalid_handle)

/* 00807130 (EAX = channel, xmm0 = volume; no result): a music channel's volume clamped to 0..1: +0x20 = the old +0x18 (through
 * the x87: a signalling NaN comes out quiet), +0x1c = +0x18 = the new volume, +0x24 = 0. */
void rx_channel_set_volume(rx_addr ch, float v) {
    if (v >= 1.0f /* 0x931f24 */) v = 1.0f;
    else if (0.0f >= v) v = 0.0f;
    uint32_t old = U32(ch, 0x18);
    if ((old & 0x7f800000u) == 0x7f800000u && (old & 0x007fffffu) != 0) old |= 0x00400000u;
    SF32(ch, 0x1c, v);
    SU32(ch, 0x20, old);
    SF32(ch, 0x24, 0.0f);
    SF32(ch, 0x18, v);
}
RX_FUNCTION(0x00807130, rx_channel_set_volume)

/* 00807170 (ECX = channel, stdcall fade time; RET 4): starts a fade of the channel's volume to full: when no fade runs
 * (+0x24 <= 0) and the volume +0x18 is neither 0 nor 1, and the current target +0x1c is strictly between 0 and 1: the old volume
 * goes to +0x20 (through the x87), the target becomes 1 and the fade time +0x24 the argument. */
void rx_channel_fade(rx_addr ch, float time) {
    if (!(F32(ch, 0x24) > 0.0f)) {
        const float c = F32(ch, 0x18);
        if (c == 0.0f || c == 1.0f /* 0x931f24 */) return;
    }
    const float t = F32(ch, 0x1c);
    if (!(t > 0.0f)) return;
    if (!(1.0f > t)) return;
    uint32_t old = U32(ch, 0x18);
    if ((old & 0x7f800000u) == 0x7f800000u && (old & 0x007fffffu) != 0) old |= 0x00400000u;
    SU32(ch, 0x20, old);
    SF32(ch, 0x1c, 1.0f);
    SF32(ch, 0x24, time);
}
RX_FUNCTION(0x00807170, rx_channel_fade)

/* ---- the name of the cannonball warning event ---------------------------------------------------------------------------- */

/* 00581350 (cdecl out; out is 0x32 bytes): the current world's name (game +0x39c0, at most 0x31 characters) with a trailing world
 * variant suffix cut off ("B", "C", "BW", "B_BW", "C_BW": the first of the five that the name ends with). */
void rx_sound_world_name(rx_addr out) {
    if (out == 0) return;
    static const rx_addr suffix[5] = {0x0094276c, 0x009419e0, 0x00942770, 0x00942774, 0x0094277c};
    const rx_addr name = U32(0, 0xa946b8) + 0x39c0;
    crt_strncpy(out, name, 0x31);
    SU8(out, 0x31, 0);
    for (int i = 0; i < 5; i++) {
        uint32_t ln = 0, lz = 0;
        while (U8(name, ln) != 0) ln++;
        while (U8(suffix[i], lz) != 0) lz++;
        rx_addr a = name + (ln - lz), b = suffix[i];   /* the tail of the same length (before the name when the suffix is longer) */
        int equal = 1;
        for (;;) {
            const uint8_t x = U8(a, 0), y = U8(b, 0);
            if (x != y) { equal = 0; break; }
            if (x == 0) break;
            a++; b++;
        }
        if (!equal) continue;
        uint32_t n = ln - lz + 1;
        if (n >= 0x32) n = 0x32;
        crt_strncpy(out, name, n - 1);
        SU8(out, n - 1, 0);
        return;
    }
}
RX_FUNCTION(0x00581350, rx_sound_world_name)

/* 00581490 (cdecl out): the world name (00581350) as a path: a trailing "/" when it is shorter than 0x30, then "/untitled/WORLD_EDGE"
 * appended when the whole fits (< 0x32). (The first test, _strnicmp of the edge name against "/", is never true: it starts with "/".) */
void rx_sound_world_path(rx_addr out) {
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(1, rx_sound_world_name(out));
    static const rx_addr slash = 0x00941e5c, edge = 0x0093f004;
    int32_t cmp;
    RACE_CORE_CALL(3, cmp = crt_strnicmp(edge, slash, 1));
    uint32_t len = 0;
    if (cmp != 0) {
        while (U8(out, len) != 0) len++;
        if (len + 2 < 0x32) {
            RACE_CORE_CALL(3, crt_strncpy(out + len, slash, 1));
            SU8(out + len, 1, 0);
        }
    }
    uint32_t lo = 0, le = 0;
    while (U8(out, lo) != 0) lo++;
    while (U8(edge, le) != 0) le++;
    if (le + lo + 1 < 0x32) {
        RACE_CORE_CALL(3, crt_strncpy(out + lo, edge, le));
        SU8(out + lo, le, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00581490, rx_sound_world_path)

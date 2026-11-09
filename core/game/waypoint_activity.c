/* MXActivityWaypointRace, handwritten from the original decompile and full
 * instruction spans. This class derives directly from Activity, not RaceStub.
 * Frozen candidate ownership is separate from the canonical core. */
#include "waypoint_activity.h"
#include "waypoint_audio.h"
#include "build_activity_int.h"
#include "gateless_race_start.h"
#include "loop_platform.h"
#include "race.h"

static uint32_t waypoint_x87_copy(uint32_t value) {
    return ((value & 0x7f800000u) == 0x7f800000u && (value & 0x7fffffu)) ? value | 0x400000u : value;
}

/* 00578260: ECX=kind,EAX=world,stack activity,RET4. */
rx_addr rx_waypoint_activity_construct(rx_addr a, uint32_t type, rx_addr world) {
    ACT_FRAME(f, 4, 3);
    ACT_CALL(1, rx_activity_construct(a, type, world));
    SU32(a, 0, 0x00943c14);
    for (uint32_t o = 0xff078; o <= 0xff088; o += 4) SU32(a, o, 0);
    /* The three independently embedded BaseScenario subclasses. */
    SU32(a, 0x114240, 0); SU32(a, 0x114244, 0); SU32(a, 0x114248, 0); SU32(a, 0x11424c, 0);
    SU32(a, 0xff0b4, 0); SU32(a, 0xff0a0, 0x00943b54); SU32(a, 0x114254, 15);
    SU32(a, 0x129410, 0); SU32(a, 0x129414, 0); SU32(a, 0x129418, 0); SU32(a, 0x12941c, 0);
    SU32(a, 0x129424, 15); SU32(a, 0x114284, 0); SU32(a, 0x114270, 0x00943b94);
    SU32(a, 0x13e5e0, 0); SU32(a, 0x13e5e4, 0); SU32(a, 0x13e5e8, 0); SU32(a, 0x13e5ec, 0);
    SU32(a, 0x13e5f4, 15); SU32(a, 0x129454, 0); SU32(a, 0x129440, 0x00943bd4);
    /* 005560f0 is the original finite kind-to-lap table. */
    uint32_t laps = (type == 6 || type == 0x21 || type == 0x30 || type == 0x31) ? 1 : 3;
    SU32(a, 0xff090, laps); SU32(a, 0xff094, U32(0, 0x009d78dc)); SU32(a, 0x22c, 1);
    rx_addr audio;
    ACT_CALLR(audio, 1, rx_reflex_aligned_alloc(0x98));
    if (audio) {
        SU32(audio, 0, 0x009429fc); SU32(audio, 0x94, 0); SU8(audio, 4, 0);
        SU32(audio, 0x50, 0); SU32(audio, 0x54, 0); SU32(audio, 0x8c, 0); SU32(audio, 0x90, 0);
        ACT_CALL(0, rx_audio_logic_vf32(audio));
        SU32(audio, 0, 0x00942e3c);
    }
    SU32(a, 0xfd8, audio); SU32(a, 0x2c8, 1);
    rx_addr gates; ACT_CALLR(gates, 1, rx_reflex_aligned_alloc(0x30));
    SU32(a, 0xff078, gates); SU32(a, 0xff07c, 0); SU32(a, 0xff080, 12);
    SU32(a, 0xff084, 12); SU32(a, 0xff088, 0x7fffffff);
    SU32(a, 0xff0b4, a); SU32(a, 0x114284, a); SU32(a, 0x129454, a);
    SU32(a, 0x13e610, 0); SU32(a, 0x13ec5c, 0); SU32(a, 0x13ec60, 0); SU32(a, 0x13ec64, 0);
    for (uint32_t o = 0; o < 0x640; o += 4) SU32(a, 0x13e61c + o, 0);
    ACT_END(f); return a;
}
RX_FUNCTION(0x00578260, rx_waypoint_activity_construct)

/* 00578470 / 00578450: typed audio and base teardown, then optional free. */
void rx_waypoint_activity_destroy(rx_addr a) {
    ACT_FRAME(f, 4, 2);
    SU32(a, 0, 0x00943c14);
    rx_addr audio = U32(a, 0xfd8);
    if (audio) {
        ACT_CALL(1, rx_activity_audio_destroy(audio));
        ACT_CALL(1, rx_reflex_aligned_free(audio));
    }
    SU32(a, 0x129440, 0x0096cea4); SU32(a, 0x114270, 0x0096cea4); SU32(a, 0xff0a0, 0x0096cea4);
    ACT_CALL(1, rx_reflex_aligned_free(U32(a, 0xff078)));
    ACT_CALL(0, rx_waypoint_activity_base_destroy(a));
    ACT_END(f);
}
RX_FUNCTION(0x00578470, rx_waypoint_activity_destroy)
rx_addr rx_waypoint_activity_sdtor(rx_addr a, uint32_t flags) {
    ACT_FRAME(f, 0, 1);
    ACT_CALL(0, rx_waypoint_activity_destroy(a));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(a));
    ACT_END(f); return a;
}
RX_FUNCTION(0x00578450, rx_waypoint_activity_sdtor)

/* 00579c10: the lap count belongs at +ff090 in this class. */
void rx_waypoint_activity_vf1(rx_addr a, rx_addr row) {
    ACT_FRAME(f, 0, 2);
    uint32_t laps = 99;
    if (!U32(0, 0x00ae1b90)) {
        ACT_FRAME(t, 0, 2);
        uint32_t col; rx_addr cell;
        ACT_CALLR(col, 1, rx_tbl_column_index(U32(row, 8), 0x0093dd64));
        ACT_CALLR(cell, 1, rx_tbl_cell_word(U32(row, 8), col, U32(row, 0)));
        laps = U32(cell, 0); ACT_END(t);
    }
    SU32(a, 0xff090, laps);
    ACT_CALL(1, rx_activity_vf1(a, row));
    ACT_END(f);
}
RX_FUNCTION(0x00579c10, rx_waypoint_activity_vf1)

/* 00578740: six event types on the already-created activity channel. */
void rx_waypoint_activity_vf4(rx_addr a) {
    ACT_FRAME(f, 0, 1);
    act_event_type(0x00592040, U32(a, 0x290), 0x0093eefc);
    act_event_type(0x00592160, U32(a, 0x290), 0);
    act_event_type(0x00592280, U32(a, 0x290), 0);
    act_event_type(0x00590f60, U32(a, 0x290), 0x0093ef30);
    act_event_type(0x00590f60, U32(a, 0x290), 0x0093eef0);
    act_event_type(0x00592040, U32(a, 0x290), 0x0093eff0);
    ACT_END(f);
}
RX_FUNCTION(0x00578740, rx_waypoint_activity_vf4)

/* 00578500: load the waypoint track/pacing, then publish loaded essentials. */
void rx_waypoint_activity_vf5(rx_addr a) {
    ACT_FRAME_EBP(f, 8, 4, 3);
    ACT_CALL(0, rx_activity_vf5(a));
    ACT_CALL(3, rx_track_load(U32(a, 0x234) + 0x20030, a + 0x890, 4, U32(a, 0x220)));
    rx_addr w = U32(a, 0x234);
    ACT_CALL(4, rx_activity_lap_pacing(w + 0x20030, a, 6, U32(a, 0x220), U32(w, 0x10)));
    SU32(f, 12, a);
    act_raise(U32(0, 0x00a93f74), 0x00936038, 0x009cbd24, f + 12, 4);
    ACT_END(f);
}
RX_FUNCTION(0x00578500, rx_waypoint_activity_vf5)

/* 0058a360: Lap Completed -> 0057a050, original typed event template. */
void rx_waypoint_subscribe_lap(rx_addr channel, rx_addr activity) {
    act_subscribe(0x0058a360, 1, 1, 2, channel, 0x0093eefc,
                  0x00945ca4, 0x009cda64, activity, 0x0057a050);
}
RX_FUNCTION(0x0058a360, rx_waypoint_subscribe_lap)

/* 005784d0: comparator deliberately returns -1 for equal gate indices. */
int32_t rx_waypoint_compare_gates(rx_addr left, rx_addr right) {
    return I32(U32(right, 0), 0x280) < I32(U32(left, 0), 0x280) ? 1 : -1;
}
RX_FUNCTION(0x005784d0, rx_waypoint_compare_gates)

/* 007d26d0: ESI=destination dynamic array, scene/type/group on stack, RET12.
 * This is the full 24-bucket gather, including its capacity-step growth. */
void rx_waypoint_scene_collect_typed(rx_addr array, rx_addr scene, uint32_t type, rx_addr group) {
    ACT_FRAME(f, 12, 3);
    loop_enter_cs(scene + 4); SU32(array, 4, 0);
    for (uint32_t b = 0; b < 24; ++b) {
        rx_addr bucket = scene + 0x38 + b * 0x110;
        if (!U32(bucket, 0x10c) || crt_strcmp(group, bucket)) continue;
        for (int32_t i = 0; i < I32(bucket, 0x108); ++i) {
            rx_addr entity = U32(U32(bucket, 0x104), 4u * (uint32_t)i);
            if (act_vcall(entity, 0x58, 0, 0) != type) continue;
            rx_addr element = U32(bucket, 0x104) + 4u * (uint32_t)i;
            uint32_t capacity = U32(array, 8);
            if (I32(array, 4) >= (int32_t)capacity) {
                uint32_t grown = capacity + U32(array, 12);
                rx_addr buf; ACT_CALLR(buf, 1, rx_reflex_aligned_alloc(grown > 0x3fffffffu ? UINT32_MAX : grown * 4));
                for (uint32_t j = 0; j < capacity * 4u; ++j) SU8(buf, j, U8(U32(array, 0), j));
                ACT_CALL(1, rx_reflex_aligned_free(U32(array, 0)));
                SU32(array, 0, buf); SU32(array, 8, grown);
            }
            SU32(U32(array, 0), U32(array, 4) * 4u, U32(element, 0));
            SU32(array, 4, U32(array, 4) + 1);
        }
    }
    loop_leave_cs(scene + 4); ACT_END(f);
}
RX_FUNCTION(0x007d26d0, rx_waypoint_scene_collect_typed)

/* 005787a0: twelve grid slots walk the latter half of the path, then choose
 * the terminal segment once the index wraps back to its starting position. */
void rx_waypoint_start_slots(rx_addr a) {
    SU32(a, 0x884, 0);
    for (uint32_t i = 0; i < 28; ++i) SU32(a, 0x360 + 48 * i, U32(a, 0x8c8));
    int32_t index = I32(a, 0xa68) / 2;
    uint32_t wrapped = 0;
    for (uint32_t i = 0; i < 12; ++i) {
        rx_addr slot = a + 0x640 + 48 * i;
        rx_addr path = U32(U32(a, 0x9c0), 4u * (uint32_t)index);
        SU32(slot, 0x20, path);
        rx_addr point = wrapped ? U32(U32(path, 4), 0x60) : U32(path, 0);
        SU32(slot, 0x24, point);
        for (uint32_t k = 0; k < 3; ++k) SU32(slot, k * 4, waypoint_x87_copy(U32(point, k * 4)));
        point = U32(slot, 0x24);
        for (uint32_t k = 0; k < 3; ++k) SU32(slot, 0x10 + k * 4, waypoint_x87_copy(U32(point, 0x50 + k * 4)));
        index = (index + 1) % I32(a, 0xa68);
        if (index == I32(a, 0xa68) / 2) wrapped = 1;
    }
    SU32(a, 0x880, 28);
}
RX_FUNCTION(0x005787a0, rx_waypoint_start_slots)

/* 00579f10: sum of the three coordinate differences, then sqrt(sum*sum).
 * Preserve this original SSE operation order, including its unusual metric. */
void rx_waypoint_gate_distances(rx_addr a) {
    ACT_FRAME_EBP(f, 16, 0x8c, 1);
    for (int32_t i = 0; i < I32(a, 0xff074); ++i) {
        uint32_t out = f + 0x10;
        act_vcall(U32(U32(a, 0xff078), 4u * (uint32_t)i), 0x3c, 1, &out);
        int32_t next = i == I32(a, 0xff074) - 1 ? 0 : i + 1;
        out = f + 0x50;
        act_vcall(U32(U32(a, 0xff078), 4u * (uint32_t)next), 0x3c, 1, &out);
        float sum = rx_adds(rx_subs(F32(f, 0x48), F32(f, 0x88)), rx_subs(F32(f, 0x44), F32(f, 0x84)));
        sum = rx_adds(sum, rx_subs(F32(f, 0x40), F32(f, 0x80)));
        SF32(U32(a, 0xff09c), 4u * (uint32_t)i, rx_sqrts(rx_muls(sum, sum)));
    }
    ACT_END(f);
}
RX_FUNCTION(0x00579f10, rx_waypoint_gate_distances)

/* 0057a7f0: the original 200 marker bound and two-byte decimal suffix. */
void rx_waypoint_collect_markers(rx_addr a) {
    ACT_FRAME(f, 0x328, 1);
    for (uint32_t i = 0; i < 0x640; i += 4) SU32(a, 0x13e61c + i, 0);
    SU32(f, 4, 0);
    for (uint32_t i = 0; i < 0x320; i += 4) SU32(f, 12 + i, 0);
    uint32_t count;
    ACT_CALLR(count, 4, rx_scene_collect_prefixed(U32(U32(a, 0x234), 0x20138), 0x009418a8, f + 12, 200, f + 4));
    SU32(f, 8, count); SU32(a, 0x13ec5c, 0);
    if ((int32_t)count > 0) {
        ACT_FRAME(t, 0, 2);
        for (int32_t i = 0; i < I32(f, 8); ++i) {
            rx_addr e = U32(f, 12 + 4u * (uint32_t)i);
            if (act_vcall(e, 0x58, 0, 0) != 0x102a) continue;
            SU8(f, 4, U8(U32(e, 0x18), 3)); SU8(f, 5, U8(U32(e, 0x18), 4)); SU8(f, 6, 0);
            int32_t number; ACT_CALLR(number, 1, crt_atoi(f + 4));
            SU32(a, 0x13e620 + 8 * U32(a, 0x13ec5c), e);
            SU32(a, 0x13e61c + 8 * U32(a, 0x13ec5c), (uint32_t)number - 1);
            SU32(a, 0x13ec5c, U32(a, 0x13ec5c) + 1);
        }
        ACT_END(t);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0057a7f0, rx_waypoint_collect_markers)

/* 00578590: initialize the separate waypoint activity and its gate arrays. */
void rx_waypoint_activity_vf2(rx_addr a, rx_addr overlay) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(f, 0, 3);
    ACT_CALL(1, rx_activity_vf2(a, overlay));
    ACT_CALL(1, rx_waypoint_subscribe_lap(U32(a, 0x290), a));
    if (U32(a, 0x23c) == 0x1f) act_unreached(0x005b6a30, "waypoint championship introduction setup");
    ACT_CALL(3, rx_waypoint_scene_collect_typed(a + 0xff078, U32(U32(a, 0x234), 0x20138), 0x1043, a + 0x118));
    uint32_t n = U32(a, 0xff07c); SU32(a, 0xff074, n);
    rx_addr distances; ACT_CALLR(distances, 1, rx_reflex_aligned_alloc(n > 0x3fffffffu ? UINT32_MAX : n * 4));
    SU32(a, 0xff09c, distances);
    /* The first cdecl allocation argument remains below ESP at the next call. */
    rx_set_sp(rx_sp() - 4);
    n = U32(a, 0xff090) * U32(a, 0xff074) + 1;
    rx_addr times; ACT_CALLR(times, 1, rx_reflex_aligned_alloc(n > 0x3fffffffu ? UINT32_MAX : n * 4));
    rx_set_sp(rx_sp() + 4); SU32(a, 0xff098, times);
    for (int32_t i = 0; i < (int32_t)(U32(a, 0xff090) * U32(a, 0xff074) + 1); ++i)
        SU32(U32(a, 0xff098), 4u * (uint32_t)i, 0);
    ACT_CALL(4, crt_qsort(U32(a, 0xff078), U32(a, 0xff07c), 4, 0x005784d0));
    ACT_CALL(0, rx_waypoint_gate_distances(a));
    ACT_CALL(0, rx_waypoint_start_slots(a));
    ACT_CALL(4, rx_gateless_race_start(a, a + 0x8a0, 12, 1));
    SU32(a, 0xff070, 0); SU32(a, 0x224, 0);
    if (U32(a, 0x238) == 3) { SU32(a, 0xff08c, U32(0, 0x00977494)); SU32(a, 0x228, 0); SU32(a, 0x224, 1); }
    if (U32(a, 0x238)) {
        SU32(entry, 4, a);
        act_raise(U32(0, 0x00a93f74), 0x00935ea4, 0x009cbd24, entry + 4, 4);
    }
    ACT_CALL(0, rx_waypoint_collect_markers(a));
    ACT_END(f);
}
RX_FUNCTION(0x00578590, rx_waypoint_activity_vf2)

#include "race_activity.h"
#include "build_activity_freeride_run.h"
#include "waypoint_driver.h"

/* 00579fd0: two independent SSE sums followed by the possible odd tail. */
float rx_waypoint_route_length(rx_addr a) {
    int32_t count = I32(a, 0xff074), i = 0;
    float even = 0.0f, odd = 0.0f, tail = 0.0f;
    if (count >= 2) {
        do {
            float v = i < count ? F32(U32(a, 0xff09c), 4u * (uint32_t)i) : 0.0f;
            even = rx_adds(v, even);
            v = i + 1 < count ? F32(U32(a, 0xff09c), 4u * (uint32_t)(i + 1)) : 0.0f;
            odd = rx_adds(v, odd);
            i += 2;
        } while (i < count - 1);
    }
    if (i < count) tail = F32(U32(a, 0xff09c), 4u * (uint32_t)i);
    return rx_adds(rx_adds(odd, even), tail);
}
RX_FUNCTION(0x00579fd0, rx_waypoint_route_length)

/* 0057a7d0: signed upper bound only, as in the original. */
rx_addr rx_waypoint_gate_at(int32_t index, rx_addr a) {
    if (index < I32(a, 0xff074)) return U32(U32(a, 0xff078), 4u * (uint32_t)index);
    return 0;
}
RX_FUNCTION(0x0057a7d0, rx_waypoint_gate_at)

/* 0057a8d0 / 0057a940: visibility is virtual slot5 of the embedded
 * interface at entity+90, following the entity's type check. */
static void marker_visible(rx_addr e, uint32_t active) {
    SU32(e, 0x70, active);
    if (act_vcall(e, 0x58, 0, 0) == 0x102a)
        act_vcall(e + 0x90, 0x14, 1, &active);
}
void rx_waypoint_set_marker(rx_addr a, int32_t index, uint32_t active) {
    ACT_FRAME(f, 0, 4);
    for (int32_t i = 0; i < I32(a, 0x13ec5c); ++i)
        if (I32(a, 0x13e61c + 8u * (uint32_t)i) == index)
            marker_visible(U32(a, 0x13e620 + 8u * (uint32_t)i), active);
    ACT_END(f);
}
RX_FUNCTION(0x0057a8d0, rx_waypoint_set_marker)
void rx_waypoint_select_marker(rx_addr a, int32_t index) {
    ACT_FRAME(f, 0, 4);
    for (int32_t i = 0; i < I32(a, 0x13ec5c); ++i) {
        rx_addr e = U32(a, 0x13e620 + 8u * (uint32_t)i);
        marker_visible(e, I32(a, 0x13e61c + 8u * (uint32_t)i) == index);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0057a940, rx_waypoint_select_marker)
void rx_waypoint_advance_markers(rx_addr a, int32_t index, rx_addr driver) {
    rx_addr player = U32(driver, 0x74);
    if (!U32(player, 4) || U32(player, 0x10)) return;
    ACT_FRAME(f, 0, 2);
    if (!U32(a, 0x13ec64)) {
        if (index >= 0) ACT_CALL(2, rx_waypoint_select_marker(a, index + 1));
    } else {
        ACT_CALL(3, rx_waypoint_set_marker(a, index + 1, 1));
        if (index >= 0) {
            rx_addr other = 0;
            if (driver == U32(a, 0x13ec60)) other = U32(a, 0x13ec64);
            else if (driver == U32(a, 0x13ec64)) other = U32(a, 0x13ec60);
            if (other && I32(other, 0x10d4) + 1 != index) ACT_CALL(3, rx_waypoint_set_marker(a, index, 0));
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x0057a9b0, rx_waypoint_advance_markers)

/* 0057a740: scenario completion controls whether the activity is skippable. */
uint32_t rx_waypoint_activity_vf8(rx_addr a) {
    ACT_FRAME(f, 0, 1);
    uint32_t result = 0;
    if (U32(a, 0x21c) == 0) {
        ACT_FRAME(t, 0, 1);
        if ((act_vcall(a + 0xff0a0, 0x2c, 0, 0) || act_vcall(a + 0xff0a0, 0x1c, 0, 0)) && !U32(a, 0x1068)) result = 1;
        ACT_END(t);
    } else if (U32(a, 0x21c) == 4) {
        rx_set_sp(f_sp + 4); /* pop saved register; tail jump reuses caller return address */
        if (U32(a, 0x13e618) == 1) result = act_vcall(a + 0x114270, 0x2c, 0, 0);
        else if (U32(a, 0x13e618) == 3) result = act_vcall(a + 0x129440, 0x2c, 0, 0);
    }
    ACT_END(f); return result;
}
RX_FUNCTION(0x0057a740, rx_waypoint_activity_vf8)

/* 00578c00: original mode-dependent grid regeneration. */
void rx_waypoint_activity_vf18(rx_addr a) {
    ACT_FRAME_EBP(f, 8, 4, 1);
    SU32(a, 0x114254, 15); SU32(a, 0x129424, 0);
    uint32_t mode = U32(a, 0x238);
    if (mode != 2 && mode != 3 && U32(a, 0x23c) == 0x1f)
        act_unreached(0x005b6a30, "waypoint championship restart");
    if (!U32(a, 0x884)) {
        rx_addr track = a + (mode == 3 ? 0x8d0 : mode == 2 ? 0x960 : 0x8a0);
        ACT_CALL(4, rx_gateless_race_start(a, track, 12, 0));
    }
    ACT_CALL(0, rx_activity_vf18(a)); ACT_END(f);
}
RX_FUNCTION(0x00578c00, rx_waypoint_activity_vf18)

/* 00578ec0: update every active embedded scenario after the base activity. */
void rx_waypoint_activity_vf23(rx_addr a, float dt) {
    ACT_FRAME(f, 0, 2);
    ACT_CALL(1, rx_activity_update(a, rx_bf(waypoint_x87_copy(rx_fb(dt)))));
    const uint32_t scenarios[3] = {0xff0a0, 0x114270, 0x129440};
    for (uint32_t i = 0; i < 3; ++i)
        if (!act_vcall(a + scenarios[i], 0x20, 0, 0)) act_vcall(a + scenarios[i], 0x38, 0, 0);
    ACT_END(f);
}
RX_FUNCTION(0x00578ec0, rx_waypoint_activity_vf23)

/* 0057be40: the class override delegates to Activity's countdown. */
void rx_waypoint_activity_vf26(rx_addr a, float dt) {
    ACT_CALL(1, rx_activity_countdown(a, rx_bf(waypoint_x87_copy(rx_fb(dt)))));
}
RX_FUNCTION(0x0057be40, rx_waypoint_activity_vf26)

/* 00579040: countdown-enter, x87 load/store quiets a signaling NaN. */
void rx_waypoint_activity_vf38(rx_addr a, uint32_t previous) {
    (void)previous;
    ACT_FRAME(f, 0, 1);
    ACT_CALL(1, rx_activity_countdown_enter(a));
    uint32_t value = U32(a, 0x2b0);
    if ((value & 0x7f800000u) == 0x7f800000u && (value & 0x7fffffu)) value |= 0x400000u;
    SU32(a, 0x29c, value); SU32(a, 0x2a0, U32(a, 0x2b8)); SU32(a, 0x2cc, 0);
    uint32_t mode = U32(a, 0x238);
    if (mode == 0 || mode - 2u < 2) SU32(a, 0x29c, 0);
    ACT_END(f);
}
RX_FUNCTION(0x00579040, rx_waypoint_activity_vf38)

/* 0057ea00: thiscall vf44(player), class-specific driver allocation. */
rx_addr rx_waypoint_activity_vf44(rx_addr a, rx_addr player) {
    (void)a;
    rx_addr driver; ACT_CALLR(driver, 1, rx_reflex_aligned_alloc(0x10e0));
    if (driver) ACT_CALLR(driver, 0, rx_waypoint_driver_construct(driver, player));
    return driver;
}
RX_FUNCTION(0x0057ea00, rx_waypoint_activity_vf44)
uint32_t rx_waypoint_activity_vf48(rx_addr a) { return U32(a, 0xff070); }
RX_FUNCTION(0x0055ebb0, rx_waypoint_activity_vf48)

/* 00579b70: the original audio/camera policy globals by activity mode. */
void rx_waypoint_activity_vf52(rx_addr a) {
    int32_t target;
    switch (U32(a, 0x238)) {
    case 1:
        SU32(0, 0x009c3edc, !U32(a, 0x234) || U32(U32(a, 0x234), 0x22148) ? 5 : 0);
        SU32(0, 0x009c3ed8, 0x3f800000); target = 2; break;
    case 2:
        SU32(0, 0x009c3edc, 0); SU32(0, 0x009c3ed8, 0x3f000000); target = 1; break;
    case 3:
        SU32(0, 0x009c3ed8, 0); SU32(0, 0x009c3edc, 0); target = -1; break;
    default: return;
    }
    if (I32(0, 0x009c47bc) != target) SU32(0, 0x009c47c0, (uint32_t)target);
}
RX_FUNCTION(0x00579b70, rx_waypoint_activity_vf52)

/* 00586060: right subtree first, free node, then iterate left subtree. */
void rx_waypoint_state_nodes_destroy(rx_addr node) {
    ACT_FRAME(f, 0, 3);
    while (!U8(node, 0x2d)) {
        ACT_CALL(1, rx_waypoint_state_nodes_destroy(U32(node, 8)));
        rx_addr left = U32(node, 0);
        ACT_CALL(0, crt_str_dtor(node + 0x10));
        ACT_CALL(1, rx_reflex_aligned_free(node));
        node = left;
    }
    ACT_END(f);
}
RX_FUNCTION(0x00586060, rx_waypoint_state_nodes_destroy)

/* 00556660: its 005863f0 call always covers [begin,end); only the complete
 * range is needed here. The separate generic partial-range function is not
 * registered or claimed by this implementation. */
void rx_waypoint_state_map_destroy(rx_addr map) {
    ACT_FRAME_EBP(f, 8, 8, 0);
    rx_addr head = U32(map, 0x18);
    if (!U32(map, 0)) act_unreached(0x00916224, "invalid activity state map proxy");
    /* 005863f0: five arguments + return address, sub8, four pushes. */
    ACT_FRAME(range, 24, 6);
    ACT_CALL(1, rx_waypoint_state_nodes_destroy(U32(head, 4)));
    SU32(head, 4, head); SU32(map, 0x1c, 0); SU32(head, 0, head); SU32(head, 8, head);
    SU32(f, 0, U32(map, 0)); SU32(f, 4, U32(head, 0));
    ACT_END(range);
    ACT_CALL(1, rx_reflex_aligned_free(U32(map, 0x18)));
    rx_addr proxy = U32(map, 0); SU32(map, 0x18, 0); SU32(map, 0x1c, 0);
    ACT_CALL(1, rx_reflex_aligned_free(proxy)); ACT_END(f);
}
RX_FUNCTION(0x00556660, rx_waypoint_state_map_destroy)

/* 005566b0: release the embedded component, track and driver arrays. */
void rx_waypoint_activity_base_destroy(rx_addr a) {
    ACT_FRAME(f, 0, 1);
    SU32(a, 0, 0x00942904);
    rx_addr component = U32(a, 0x106c);
    if (component) {
        act_vcall(component, 0x24, 0, 0);
        component = U32(a, 0x106c);
        if (component) { uint32_t flags = 1; act_vcall(component, 0, 1, &flags); }
        SU32(a, 0x106c, 0);
    }
    ACT_CALL(0, rx_track_block_reset(a + 0x890));
    ACT_CALL(1, rx_reflex_aligned_free(U32(a, 0x27c)));
    ACT_CALL(1, rx_reflex_aligned_free(U32(a, 0x268)));
    ACT_CALL(0, rx_waypoint_state_map_destroy(a + 0x240)); ACT_END(f);
}
RX_FUNCTION(0x005566b0, rx_waypoint_activity_base_destroy)

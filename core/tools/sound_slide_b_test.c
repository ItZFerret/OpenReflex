/* sound_slide_b_test.c -- the RX_SCF_SLIDE_B contacts carry A's slide input (w-reverb2; 0067c670 runs the same A switch as 0067c350).
 *   sound_slide_b_test <install root> [level (NAT_Bedford)]
 * Loads a National race, races until the sound query reports a contact on a class 5 collision entity, then (between frames) gives that entity the
 * slide flag (+ 0x19c) and a material (+ 0x158), hands rx_sound_record_contacts a synthetic event block (A = the local vehicle, then A = the entity
 * itself) and reads the contacts back through rx_session_sound: SLIDE_B set, object_material, slide_kind 3 with the vehicle's + 0x150 speed for a
 * vehicle, kind 0 for a collision entity. The entity's words are restored. Also prints rx_session_first_person_view. Test only. Exit 0 = pass. */
#include "rx_api.h"
#include "rx_game.h"
#include "rx_sound.h"
#include "sound_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_sound_state v;
static int query(rx_session* s) { memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_SOUND_VERSION; return rx_session_sound(s, &v); }
static const rx_sound_contact* newest(void) { return &v.contacts[v.contact_serial % RX_SOUND_CONTACTS]; }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <install root> [level]\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.location, sizeof e.location, "%s", argc > 2 ? argv[2] : "NAT_Bedford");
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    uint32_t ent = 0, veh = 0; int racing = 0;
    for (int i = 0; i < 12000 && racing < 900 && !ent; i++) {
        const rx_session_state st = rx_session_get_state(s);
        uint8_t keys[256] = {0};
        if (st == RX_SESSION_RACING) { racing++; keys[0x11] = 0x80; }
        if (rx_session_set_input(s, keys, NULL, 0) || rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); return 1; }
        if (query(s) || !v.valid) continue;
        for (uint32_t k = 0; k < RX_SOUND_CONTACTS; k++) if (v.contacts[k].serial && v.contacts[k].class_b == RX_SC_COLLISION_ENTITY && v.contacts[k].b) ent = v.contacts[k].b;
        if (v.rider_count > 0) veh = v.riders[0].vehicle;
    }
    printf("first-person view %d\n", rx_session_first_person_view(s));
    if (!ent || !veh) { fprintf(stderr, "no collision entity contact / vehicle found (ent %x veh %x)\n", ent, veh); return 1; }
    const uint8_t was_flag = U8(ent, 0x19c); const int32_t was_mat = I32(ent, 0x158);
    SU8(ent, 0x19c, 1); SU32(ent, 0x158, 2);
    const rx_addr sp = rx_sp(), block = (sp - 0x10000u) & ~15u;
    int fails = 0;
    for (int pass = 0; pass < 2; pass++) {
        const uint32_t a = pass == 0 ? veh : ent;
        memset(&v, 0, sizeof v);
        for (uint32_t k = 0; k < 0x1940; k += 4) SU32(block, k, 0);
        SU32(block, 0, a); SU32(block, 4, ent); SU32(block, 0x30, 0xffffffffu); SU32(block, 0x34, 0xffffffffu); SU32(block, 0x1900, 1);
        rx_sound_record_contacts(block);
        rx_set_sp(sp);
        if (query(s)) { fprintf(stderr, "query failed\n"); return 1; }
        const rx_sound_contact* c = newest();
        printf("A=%s: class %x/%x flags %x slide_kind %d speed %.2f (vehicle +0x150 %.2f) object_material %d\n", pass == 0 ? "vehicle" : "entity", c->class_a, c->class_b, c->flags,
               c->slide_kind, c->slide_speed, F32(veh, 0x150), c->object_material);
        if (!(c->flags & RX_SCF_SLIDE_B) || c->object_material != 2) { puts("FAIL: SLIDE_B / material"); fails++; }
        if (pass == 0 && (c->slide_kind != 3 || c->slide_speed != F32(veh, 0x150))) { puts("FAIL: vehicle slide input"); fails++; }
        if (pass == 1 && c->slide_kind != 0) { puts("FAIL: entity A must give kind 0"); fails++; }
    }
    SU8(ent, 0x19c, was_flag); SU32(ent, 0x158, (uint32_t)was_mat);
    rx_session_destroy(s);
    puts(fails ? "FAILED" : "PASS");
    return fails ? 1 : 0;
}

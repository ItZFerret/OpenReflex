/* crowd_flash_session.inc -- rx_session_crowd_flashes (include/rx_crowd_api.h); included by rx_session.c (w-screens, 2026-10-27).
 * Read-only: the flash records of every BillboardCrowdEntity the ambient scan found, as 006c6650 keeps them (entity +0x17c .. +0x180,
 * 16 bytes each: +0 active byte, +4 age, +8 length, +0xc seat number). The original queues a record when it is active (006c6340 -> 006c7fe0)
 * and draws it in XenonBillboardCrowdRenderEvent 006c7880. */

int rx_session_crowd_flashes(struct rx_session* s, rx_crowd_flash* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    ambient_ensure(s);
    uint32_t total = 0;
    for (int32_t i = 0; i < s->ambient_objects_count; i++) {
        const struct rx_ambient_object* o = &s->ambient_objects[i];
        if (o->kind != AK_BILLBOARD) continue;
        const rx_addr a = o->address;
        if (U32(a, 0) != k_ambient_kinds[AK_BILLBOARD].vt0 || U32(a, 0x90) != k_ambient_kinds[AK_BILLBOARD].vt90) continue;   /* gone */
        const rx_addr begin = U32(a, 0x17c), end = U32(a, 0x180);
        if (!begin || end < begin) continue;
        const uint32_t n = (end - begin) >> 4;
        const float ratio = crowd_empty_chair_ratio(a);
        for (uint32_t k = 0; k < n && k < 1024u; k++) {
            const rx_addr f = begin + 16u * k;
            if (U8(f, 0) == 0) continue;
            if (out && total < max) {
                rx_crowd_flash* r = &out[total];
                memset(r, 0, sizeof *r);
                r->object_id = a;
                r->entity_id = I32(a, 0x14);
                r->binding = (int32_t)k;
                r->seat = U32(f, 0xc);
                r->age = F32(f, 4);
                r->length = F32(f, 8);
                r->empty_chair_ratio = ratio;
            }
            total++;
        }
    }
    return (int)total;
}

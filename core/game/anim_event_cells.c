/* anim_event_cells.c -- the window growers of an event cell's lists (core/game/anim_scene.c's cell_push: a list is a ring of 0x20-byte event records at
 * the cell's storage with a head and a tail index and a count; a full window grows up through the parent cell (+0x54) or down through the child (+0x50)
 * recursively). Each list has its own fields: count / head / tail / storage +0x38 / 0x3c / 0x40 / 0x58, +0x44 / 0x48 / 0x4c / 0x5c, +0x20 / 0x24 / 0x28 /
 * 0x60, +0x2c / 0x30 / 0x34 / 0x64. Hand transliterations of 007d5d20 .. 007d5e70 (up) and 007d5ee0 .. 007d60f0 (down). */
#include "anim.h"
#include "anim_extern.h"
#include "anim_event_cells.h"

typedef struct { uint32_t cnt, head, tail, data; uint32_t (*up)(rx_addr), (*down)(rx_addr); } cell_list;
uint32_t rx_event_cell_grow_up_l38(rx_addr c); uint32_t rx_event_cell_grow_up_l44(rx_addr c);
uint32_t rx_event_cell_grow_up_l20(rx_addr c); uint32_t rx_event_cell_grow_up_a(rx_addr c);
uint32_t rx_event_cell_grow_down_l38(rx_addr c); uint32_t rx_event_cell_grow_down_l44(rx_addr c);
uint32_t rx_event_cell_grow_down_l20(rx_addr c); uint32_t rx_event_cell_grow_down_a(rx_addr c);

/* a record copy: the id word, two floats through the x87 (fld / fstp: a signalling NaN comes out quiet), a word, 16 bytes with movaps */
static void rec_copy(rx_addr dst, rx_addr src) {
    SU32(dst, 0, U32(src, 0));
    anim_x87_copy_f32(dst + 4, src + 4);
    anim_x87_copy_f32(dst + 8, src + 8);
    SU32(dst, 0xc, U32(src, 0xc));
    rx_st4(dst + 0x10, rx_ld4(src + 0x10));
}

/* up: when the window (tail - head + 1) is larger than the count the head moves up; else the parent grows (and the tail takes the head's record) */
static uint32_t grow_up(rx_addr c, const cell_list* l) {
    const uint32_t head = U32(c, l->head);
    if ((uint32_t)(U32(c, l->tail) - head + 1u) > U32(c, l->cnt)) { SU32(c, l->head, head + 1); return 1; }
    const rx_addr parent = U32(c, 0x54);
    if (parent && l->up(parent)) {
        SU32(c, l->tail, U32(c, l->tail) + 1);
        rec_copy(U32(c, l->data) + 32u * U32(c, l->tail), U32(c, l->data) + 32u * U32(c, l->head));
        SU32(c, l->head, U32(c, l->head) + 1);
        return 1;
    }
    return 0;
}

/* down: when the window is larger than the count the tail's record moves count slots down and the tail drops; else the child grows (the head drops, the
 * tail's record moves to it and the tail drops) */
static uint32_t grow_down(rx_addr c, const cell_list* l) {
    const uint32_t tail = U32(c, l->tail), cnt = U32(c, l->cnt);
    if ((uint32_t)(tail - U32(c, l->head) + 1u) > cnt) {
        rec_copy(U32(c, l->data) + 32u * (tail - cnt), U32(c, l->data) + 32u * tail);
        SU32(c, l->tail, U32(c, l->tail) - 1);
        return 1;
    }
    const rx_addr child = U32(c, 0x50);
    if (child && l->down(child)) {
        const uint32_t t = U32(c, l->tail);
        SU32(c, l->head, U32(c, l->head) - 1);
        rec_copy(U32(c, l->data) + 32u * U32(c, l->head), U32(c, l->data) + 32u * t);
        SU32(c, l->tail, U32(c, l->tail) - 1);
        return 1;
    }
    return 0;
}

static const cell_list L38 = {0x38, 0x3c, 0x40, 0x58, rx_event_cell_grow_up_l38, rx_event_cell_grow_down_l38};
static const cell_list L44 = {0x44, 0x48, 0x4c, 0x5c, rx_event_cell_grow_up_l44, rx_event_cell_grow_down_l44};
static const cell_list L20 = {0x20, 0x24, 0x28, 0x60, rx_event_cell_grow_up_l20, rx_event_cell_grow_down_l20};
static const cell_list LA  = {0x2c, 0x30, 0x34, 0x64, rx_event_cell_grow_up_a, rx_event_cell_grow_down_a};

uint32_t rx_event_cell_grow_up_l38(rx_addr c) { return grow_up(c, &L38); }
RX_FUNCTION(0x007d5d20, rx_event_cell_grow_up_l38)
uint32_t rx_event_cell_grow_up_l44(rx_addr c) { return grow_up(c, &L44); }
RX_FUNCTION(0x007d5d90, rx_event_cell_grow_up_l44)
uint32_t rx_event_cell_grow_up_l20(rx_addr c) { return grow_up(c, &L20); }
RX_FUNCTION(0x007d5e00, rx_event_cell_grow_up_l20)
uint32_t rx_event_cell_grow_up_a(rx_addr c) { return grow_up(c, &LA); }
RX_FUNCTION(0x007d5e70, rx_event_cell_grow_up_a)
uint32_t rx_event_cell_grow_down_l38(rx_addr c) { return grow_down(c, &L38); }
RX_FUNCTION(0x007d5ee0, rx_event_cell_grow_down_l38)
uint32_t rx_event_cell_grow_down_l44(rx_addr c) { return grow_down(c, &L44); }
RX_FUNCTION(0x007d5f90, rx_event_cell_grow_down_l44)
uint32_t rx_event_cell_grow_down_l20(rx_addr c) { return grow_down(c, &L20); }
RX_FUNCTION(0x007d6040, rx_event_cell_grow_down_l20)
uint32_t rx_event_cell_grow_down_a(rx_addr c) { return grow_down(c, &LA); }
RX_FUNCTION(0x007d60f0, rx_event_cell_grow_down_a)

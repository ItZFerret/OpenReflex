/* Handwritten native semantic owner of the two original 4-byte sort templates:
 *005c1980 comparator rows and005c1a80 signed ints. Shared kernel retains median
 * sampling, equal-pivot partition, smaller-side recursion, ideal-budget decay,
 * heap fallback and insertion order. No host qsort/std::sort substitute.
 * Single's finite row comparer is005b0590; no arbitrary original callback. */
#include "career_single.h"
#include "crt.h"

static uint32_t less(uint32_t a, uint32_t b, const rx_career_single_dependencies* rows) {
    return rows ? rx_career_gate_row_less(a, b, rows) : (int32_t)a < (int32_t)b;
}
static uint32_t at(rx_addr p) { return U32(p, 0); }
static void put(rx_addr p, uint32_t x) { SU32(p, 0, x); }
static void swap(rx_addr a, rx_addr b) {
    if (a != b) { uint32_t t = at(a); put(a, at(b)); put(b, t); }
}

/*005c2f70 comparator and inlined signed counterpart005c2ba0. */
static void median_three(rx_addr first, rx_addr mid, rx_addr last, const rx_career_single_dependencies* rows) {
    if (less(at(mid), at(first), rows)) swap(mid, first);
    if (less(at(last), at(mid), rows)) swap(last, mid);
    if (less(at(mid), at(first), rows)) swap(mid, first);
}
static void median_guess(rx_addr first, rx_addr mid, rx_addr last, const rx_career_single_dependencies* rows) {
    const uint32_t span = (last - first) / 4;
    if (span > 40) {
        const uint32_t step = ((span + 1) / 8) * 4;
        median_three(first, first + step, first + 2 * step, rows);
        median_three(mid - step, mid, mid + step, rows);
        median_three(last - 2 * step, last - step, last, rows);
        median_three(first + step, mid, last - step, rows);
    } else median_three(first, mid, last, rows);
}

/*005c2200 /005c23d0: [pivotFirst,pivotLast) is the equal group.
 * Scan comparison order and the rotations at exhausted ends are intentional. */
static void partition(rx_addr first, rx_addr last, rx_addr* lower, rx_addr* upper,
                      const rx_career_single_dependencies* rows) {
    rx_addr pivot_first = first + ((last - first) / 8) * 4;
    median_guess(first, pivot_first, last - 4, rows);
    rx_addr pivot_last = pivot_first + 4;
    while (first < pivot_first && !less(at(pivot_first - 4), at(pivot_first), rows) &&
           !less(at(pivot_first), at(pivot_first - 4), rows)) pivot_first -= 4;
    while (pivot_last < last && !less(at(pivot_last), at(pivot_first), rows) &&
           !less(at(pivot_first), at(pivot_last), rows)) pivot_last += 4;
    rx_addr scan_right = pivot_last, scan_left = pivot_first;
    for (;;) {
        for (; scan_right < last; scan_right += 4) {
            if (less(at(pivot_first), at(scan_right), rows)) continue;
            if (less(at(scan_right), at(pivot_first), rows)) break;
            swap(pivot_last, scan_right); pivot_last += 4;
        }
        while (first < scan_left) {
            scan_left -= 4;
            if (less(at(scan_left), at(pivot_first), rows)) continue;
            if (less(at(pivot_first), at(scan_left), rows)) break;
            pivot_first -= 4; swap(pivot_first, scan_left);
        }
        if (scan_left == first && scan_right == last) {
            *lower = pivot_first; *upper = pivot_last; return;
        }
        if (scan_left == first) {
            if (pivot_last != scan_right && pivot_first != pivot_last) swap(pivot_first, pivot_last);
            pivot_last += 4;
            swap(pivot_first, scan_right); pivot_first += 4; scan_right += 4;
        } else if (scan_right == last) {
            scan_left -= 4; pivot_first -= 4;
            swap(scan_left, pivot_first);
            pivot_last -= 4; swap(pivot_first, pivot_last);
        } else {
            scan_left -= 4; swap(scan_right, scan_left); scan_right += 4;
        }
    }
}

/*005c2fd0 +005c3210 /005c3040: descend to bottom, then push saved
 * value back up to its original top. Equal children choose the right child. */
static void adjust_heap(rx_addr first, uint32_t hole, uint32_t count, uint32_t value,
                        const rx_career_single_dependencies* rows) {
    const uint32_t top = hole;
    uint32_t child = 2 * hole + 2;
    while (child < count) {
        if (less(at(first + 4 * child), at(first + 4 * (child - 1)), rows)) --child;
        put(first + 4 * hole, at(first + 4 * child));
        hole = child; child = 2 * hole + 2;
    }
    if (child == count) {
        put(first + 4 * hole, at(first + 4 * (count - 1))); hole = count - 1;
    }
    while (top < hole) {
        const uint32_t parent = (hole - 1) / 2;
        if (!less(at(first + 4 * parent), value, rows)) break;
        put(first + 4 * hole, at(first + 4 * parent)); hole = parent;
    }
    put(first + 4 * hole, value);
}
static void heap_sort(rx_addr first, rx_addr last, const rx_career_single_dependencies* rows) {
    uint32_t count = (last - first) / 4;
    for (uint32_t hole = count / 2; hole > 0;) {
        --hole; adjust_heap(first, hole, count, at(first + 4 * hole), rows);
    }
    while (count > 1) {
        const uint32_t value = at(first + 4 * (count - 1));
        put(first + 4 * (count - 1), at(first));
        --count; adjust_heap(first, 0, count, value, rows);
    }
}

/*005c2b00 /005c2d30 retain the first-element sentinel, exact memmove_s
 * for a new minimum, and otherwise the unguarded predecessor comparison. */
static void insertion_sort(rx_addr first, rx_addr last, const rx_career_single_dependencies* rows) {
    for (rx_addr current = first + 4; current != last; current += 4) {
        const uint32_t value = at(current);
        if (less(value, at(first), rows)) {
            const uint32_t bytes = current - first;
            if (bytes) (void)crt_memmove_s(first + 4, bytes, first, bytes);
            put(first, value);
        } else {
            rx_addr hole = current;
            while (less(value, at(hole - 4), rows)) {
                put(hole, at(hole - 4)); hole -= 4;
            }
            put(hole, value);
        }
    }
}
static void sort_loop(rx_addr first, rx_addr last, int32_t ideal, const rx_career_single_dependencies* rows) {
    for (;;) {
        const int32_t count = (int32_t)(last - first) / 4;
        if (count <= 32) {
            if (count > 1 && first != last) insertion_sort(first, last, rows);
            return;
        }
        if (ideal < 1) { heap_sort(first, last, rows); return; }
        rx_addr lower, upper;
        partition(first, last, &lower, &upper, rows);
        ideal = ideal / 2 + (ideal / 2) / 2;
        if (lower - first < last - upper) {
            sort_loop(first, lower, ideal, rows); first = upper;
        } else {
            sort_loop(upper, last, ideal, rows); last = lower;
        }
    }
}
void rx_career_sort_gate_rows(rx_addr first, rx_addr last, int32_t ideal, const rx_career_single_dependencies* deps) {
    sort_loop(first, last, ideal, deps);
}
void rx_career_sort_gate_values(rx_addr first, rx_addr last, int32_t ideal) {
    sort_loop(first, last, ideal, 0);
}

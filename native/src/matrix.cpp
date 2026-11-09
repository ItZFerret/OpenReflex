#include "reflex_core.h"
#include "fp32.h"

#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_matrix_input) == 256 && sizeof(reflex_matrix_output) == 32,
              "ABI layout changed");
static_assert(offsetof(reflex_matrix_input, source) == 192
              && offsetof(reflex_matrix_input, destination) == 224, "ABI offsets changed");
static_assert(alignof(reflex_matrix_input) == 4 && alignof(reflex_matrix_output) == 4,
              "ABI alignment changed");

uint32_t REFLEX_CALL reflex_matrix_subtract(
    const reflex_matrix_input *input, uint32_t input_size,
    reflex_matrix_output *output, uint32_t output_size) {
    if (!input || !output) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input) || output_size != sizeof(*output)) return REFLEX_ABI_MISMATCH;
    if ((reinterpret_cast<uintptr_t>(input) % alignof(reflex_matrix_input)) != 0
        || (reinterpret_cast<uintptr_t>(output) % alignof(reflex_matrix_output)) != 0)
        return REFLEX_INVALID_INPUT;
    if (!reflex_detail::supported_fp()) return REFLEX_UNSUPPORTED_FP;
    // All native products finish before the first destination store. Snapshot
    // first to represent aliases and to keep late failures atomic, even when
    // the public output points into the input object.
    reflex_matrix_input in;
    std::memcpy(&in, input, sizeof(in));
    for (const auto& row : in.rows)
        for (float word : row) if (!reflex_detail::finite(word)) return REFLEX_INVALID_INPUT;
    for (const auto& vector : in.source)
        for (float word : vector) if (!reflex_detail::finite(word)) return REFLEX_INVALID_INPUT;
    for (const auto& vector : in.destination)
        for (float word : vector) if (!reflex_detail::finite(word)) return REFLEX_INVALID_INPUT;

    float dots[4][3];
    for (unsigned block = 0; block < 4; ++block) {
        const auto& vector = in.source[block % 2];
        for (unsigned row = 0; row < 3; ++row) {
            float products[3];
            for (unsigned lane = 0; lane < 3; ++lane) {
                products[lane] = reflex_detail::mul(in.rows[block * 3 + row][lane], vector[lane]);
                if (!reflex_detail::finite(products[lane])) return REFLEX_NUMERIC_RANGE;
            }
            const float xy = reflex_detail::add(products[0], products[1]);
            if (!reflex_detail::finite(xy)) return REFLEX_NUMERIC_RANGE;
            dots[block][row] = reflex_detail::add(xy, products[2]);
            if (!reflex_detail::finite(dots[block][row])) return REFLEX_NUMERIC_RANGE;
        }
    }
    reflex_matrix_output complete;
    for (unsigned index = 0; index < 2; ++index) {
        for (unsigned lane = 0; lane < 4; ++lane) {
            const unsigned dot_lane = lane < 3 ? lane : 2;
            const float correction = reflex_detail::add(dots[index * 2][dot_lane], dots[index * 2 + 1][dot_lane]);
            if (!reflex_detail::finite(correction)) return REFLEX_NUMERIC_RANGE;
            complete.destination[index][lane] = reflex_detail::sub(in.destination[index][lane], correction);
            if (!reflex_detail::finite(complete.destination[index][lane])) return REFLEX_NUMERIC_RANGE;
        }
    }
    std::memcpy(output, &complete, sizeof(complete));
    return REFLEX_OK;
}

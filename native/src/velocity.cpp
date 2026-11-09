#include "reflex_core.h"
#include "fp32.h"

#include <cstring>

static_assert(sizeof(reflex_velocity_input) == 8, "ABI layout changed");
static_assert(sizeof(reflex_velocity_node_input) == 96, "ABI layout changed");
static_assert(sizeof(reflex_velocity_node_output) == 64, "ABI layout changed");
static_assert(alignof(reflex_velocity_input) == 4 && alignof(reflex_velocity_node_input) == 4
              && alignof(reflex_velocity_node_output) == 4, "ABI alignment changed");

namespace {

bool overlap(const void* a, uint32_t a_size, const void* b, uint32_t b_size) noexcept {
    const auto aa = reinterpret_cast<uintptr_t>(a);
    const auto bb = reinterpret_cast<uintptr_t>(b);
    return aa <= bb ? bb - aa < a_size : aa - bb < b_size;
}

bool finite_vector(const float (&vector)[4]) noexcept {
    for (float value : vector) if (!reflex_detail::finite(value)) return false;
    return true;
}

} // namespace

uint32_t REFLEX_CALL reflex_velocity_integrate(
    const reflex_velocity_input *input, uint32_t input_size,
    const reflex_velocity_node_input *nodes, uint32_t nodes_size,
    reflex_velocity_node_output *output, uint32_t output_size) {
    if (!input) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input)) return REFLEX_ABI_MISMATCH;
    const reflex_velocity_input in = *input;
    if (in.node_count < 0 || in.node_count > static_cast<int32_t>(REFLEX_VELOCITY_MAX_NODES))
        return REFLEX_INVALID_INPUT;
    const auto count = static_cast<uint32_t>(in.node_count);
    if (nodes_size != count * sizeof(*nodes) || output_size != count * sizeof(*output))
        return REFLEX_ABI_MISMATCH;
    if (count == 0) return REFLEX_OK;
    if (!nodes || !output) return REFLEX_NULL_ARGUMENT;
    if (overlap(output, output_size, nodes, nodes_size)
        || overlap(output, output_size, input, input_size)) return REFLEX_INVALID_INPUT;
    if (!reflex_detail::supported_fp()) return REFLEX_UNSUPPORTED_FP;
    if (!reflex_detail::finite(in.dt)) return REFLEX_INVALID_INPUT;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        if (!finite_vector(node.linear_velocity) || !finite_vector(node.reciprocal_mass)
            || !finite_vector(node.linear_force) || !finite_vector(node.angular_velocity)
            || !finite_vector(node.angular_coefficient) || !finite_vector(node.angular_force))
            return REFLEX_INVALID_INPUT;
    }

    // Bounded scratch ensures even a late node/lane overflow cannot publish a
    // partial update. Native writes each vector immediately; successful finite
    // results agree, but this API deliberately makes rejected calls atomic.
    reflex_velocity_node_output complete[REFLEX_VELOCITY_MAX_NODES];
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        auto& result = complete[i];
        for (uint32_t lane = 0; lane < 4; ++lane)
            result.linear_accel[lane] = reflex_detail::mul(node.linear_force[lane], node.reciprocal_mass[lane]);
        for (uint32_t lane = 0; lane < 4; ++lane)
            result.angular_accel[lane] = reflex_detail::mul(node.angular_force[lane], node.angular_coefficient[lane]);
        if (!finite_vector(result.linear_accel) || !finite_vector(result.angular_accel))
            return REFLEX_NUMERIC_RANGE;
        for (uint32_t lane = 0; lane < 4; ++lane) {
            const float delta = reflex_detail::mul(result.linear_accel[lane], in.dt);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.linear_velocity[lane] = reflex_detail::add(delta, node.linear_velocity[lane]);
        }
        for (uint32_t lane = 0; lane < 4; ++lane) {
            const float delta = reflex_detail::mul(in.dt, result.angular_accel[lane]);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.angular_velocity[lane] = reflex_detail::add(delta, node.angular_velocity[lane]);
        }
        if (!finite_vector(result.linear_velocity) || !finite_vector(result.angular_velocity))
            return REFLEX_NUMERIC_RANGE;
    }
    std::memcpy(output, complete, output_size);
    return REFLEX_OK;
}

#include "reflex_core.h"
#include "fp32.h"

#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_force_input) == 40, "ABI layout changed");
static_assert(offsetof(reflex_force_input, node_count) == 4
              && offsetof(reflex_force_input, gravity) == 8
              && offsetof(reflex_force_input, matrix_global) == 24, "ABI layout changed");
static_assert(sizeof(reflex_force_node_input) == 112, "ABI layout changed");
static_assert(sizeof(reflex_force_node_output) == 32, "ABI layout changed");
static_assert(alignof(reflex_force_input) == 4 && alignof(reflex_force_node_input) == 4
              && alignof(reflex_force_node_output) == 4, "ABI alignment changed");

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

uint32_t REFLEX_CALL reflex_force_compose(
    const reflex_force_input *input, uint32_t input_size,
    const reflex_force_node_input *nodes, uint32_t nodes_size,
    reflex_force_node_output *output, uint32_t output_size) {
    if (!input) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input)) return REFLEX_ABI_MISMATCH;
    const reflex_force_input in = *input;
    if (in.node_count < 0 || in.node_count > static_cast<int32_t>(REFLEX_FORCE_MAX_NODES))
        return REFLEX_INVALID_INPUT;
    const auto count = static_cast<uint32_t>(in.node_count);
    if (nodes_size != count * sizeof(*nodes) || output_size != count * sizeof(*output))
        return REFLEX_ABI_MISMATCH;
    if (count == 0) return REFLEX_OK;
    if (!nodes || !output) return REFLEX_NULL_ARGUMENT;
    if (overlap(output, output_size, nodes, nodes_size)
        || overlap(output, output_size, input, input_size)) return REFLEX_INVALID_INPUT;
    if (!reflex_detail::supported_fp()) return REFLEX_UNSUPPORTED_FP;
    if (!reflex_detail::finite(in.dt) || in.dt == 0.0f
        || !finite_vector(in.gravity) || !finite_vector(in.matrix_global)) return REFLEX_INVALID_INPUT;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        if (!finite_vector(node.mass) || !finite_vector(node.accumulated_linear_force)
            || !finite_vector(node.linear_impulse) || !finite_vector(node.angular_velocity)
            || !finite_vector(node.angular_coefficient) || !finite_vector(node.accumulated_angular_force)
            || !finite_vector(node.angular_impulse)) return REFLEX_INVALID_INPUT;
    }

    const float inverse_dt = reflex_detail::div(1.0f, in.dt); // 85ca5c, once before both loops.
    if (!reflex_detail::finite(inverse_dt)) return REFLEX_NUMERIC_RANGE;
    reflex_force_node_output complete[REFLEX_FORCE_MAX_NODES];
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        auto& result = complete[i];
        for (unsigned lane = 0; lane < 4; ++lane) {
            const float gravity_force = reflex_detail::mul(node.mass[lane], in.gravity[lane]);
            if (!reflex_detail::finite(gravity_force)) return REFLEX_NUMERIC_RANGE;
            result.linear_force[lane] = reflex_detail::add(gravity_force, node.accumulated_linear_force[lane]);
        }
        if (!finite_vector(result.linear_force)) return REFLEX_NUMERIC_RANGE;

        // 857ff0 forms a skew-like matrix with live global diagonal values.
        // Zero subtraction is intentional: it differs from a sign-bit flip
        // when an angular velocity lane is positive zero.
        const auto& angular = node.angular_velocity;
        float momentum[3];
        for (unsigned lane = 0; lane < 3; ++lane)
            momentum[lane] = reflex_detail::mul(node.angular_coefficient[lane], angular[lane]);
        for (float value : momentum) if (!reflex_detail::finite(value)) return REFLEX_NUMERIC_RANGE;
        const float matrix[3][3] = {
            {in.matrix_global[0], reflex_detail::sub(0.0f, angular[2]), angular[1]},
            {angular[2], in.matrix_global[1], reflex_detail::sub(0.0f, angular[0])},
            {reflex_detail::sub(0.0f, angular[1]), angular[0], in.matrix_global[2]}
        };
        float gyro[3];
        for (unsigned row = 0; row < 3; ++row) {
            float products[3];
            for (unsigned lane = 0; lane < 3; ++lane)
                products[lane] = reflex_detail::mul(matrix[row][lane], momentum[lane]);
            for (float value : products) if (!reflex_detail::finite(value)) return REFLEX_NUMERIC_RANGE;
            const float xy = reflex_detail::add(products[0], products[1]);
            if (!reflex_detail::finite(xy)) return REFLEX_NUMERIC_RANGE;
            gyro[row] = reflex_detail::add(xy, products[2]);
            if (!reflex_detail::finite(gyro[row])) return REFLEX_NUMERIC_RANGE;
        }
        // 854490 returns (dot0, dot1, dot2, dot2). Its fourth-lane products
        // and momentum W are discarded; their FP exception effects are outside
        // this finite contributing-intermediate contract.
        for (unsigned lane = 0; lane < 4; ++lane)
            result.angular_force[lane] = reflex_detail::sub(node.accumulated_angular_force[lane], gyro[lane < 3 ? lane : 2]);
        if (!finite_vector(result.angular_force)) return REFLEX_NUMERIC_RANGE;
    }
    // Preserve the original second pass, but publish only after every node
    // succeeds so a late range error leaves the caller's whole output untouched.
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        auto& result = complete[i];
        for (unsigned lane = 0; lane < 4; ++lane) {
            const float delta = reflex_detail::mul(node.linear_impulse[lane], inverse_dt);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.linear_force[lane] = reflex_detail::add(delta, result.linear_force[lane]);
        }
        for (unsigned lane = 0; lane < 4; ++lane) {
            const float delta = reflex_detail::mul(node.angular_impulse[lane], inverse_dt);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.angular_force[lane] = reflex_detail::add(delta, result.angular_force[lane]);
        }
        if (!finite_vector(result.linear_force) || !finite_vector(result.angular_force)) return REFLEX_NUMERIC_RANGE;
    }
    std::memcpy(output, complete, output_size);
    return REFLEX_OK;
}

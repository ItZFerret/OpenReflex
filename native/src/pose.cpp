#include "reflex_core.h"
#include "fp32.h"

#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_pose_input) == 12, "ABI layout changed");
static_assert(offsetof(reflex_pose_input, node_count) == 4
              && offsetof(reflex_pose_input, angular_w) == 8, "ABI layout changed");
static_assert(sizeof(reflex_pose_node_input) == 64, "ABI layout changed");
static_assert(sizeof(reflex_pose_node_output) == 32, "ABI layout changed");
static_assert(alignof(reflex_pose_input) == 4 && alignof(reflex_pose_node_input) == 4
              && alignof(reflex_pose_node_output) == 4, "ABI alignment changed");

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

uint32_t REFLEX_CALL reflex_pose_integrate(
    const reflex_pose_input *input, uint32_t input_size,
    const reflex_pose_node_input *nodes, uint32_t nodes_size,
    reflex_pose_node_output *output, uint32_t output_size) {
    if (!input) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input)) return REFLEX_ABI_MISMATCH;
    const reflex_pose_input in = *input;
    if (in.node_count < 0 || in.node_count > static_cast<int32_t>(REFLEX_POSE_MAX_NODES))
        return REFLEX_INVALID_INPUT;
    const auto count = static_cast<uint32_t>(in.node_count);
    if (nodes_size != count * sizeof(*nodes) || output_size != count * sizeof(*output))
        return REFLEX_ABI_MISMATCH;
    if (count == 0) return REFLEX_OK;
    if (!nodes || !output) return REFLEX_NULL_ARGUMENT;
    if (overlap(output, output_size, nodes, nodes_size)
        || overlap(output, output_size, input, input_size)) return REFLEX_INVALID_INPUT;
    if (!reflex_detail::supported_fp()) return REFLEX_UNSUPPORTED_FP;
    if (!reflex_detail::finite(in.dt) || !reflex_detail::finite(in.angular_w)) return REFLEX_INVALID_INPUT;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        if (!finite_vector(node.position) || !finite_vector(node.linear_velocity)
            || !finite_vector(node.orientation) || !finite_vector(node.angular_velocity))
            return REFLEX_INVALID_INPUT;
    }

    // Original 85d10a rounds this once before either node loop.
    const float half_dt = reflex_detail::mul(in.dt, 0.5f);
    reflex_pose_node_output complete[REFLEX_POSE_MAX_NODES];
    constexpr unsigned map_x[4] = {3, 2, 1, 0};
    constexpr unsigned map_y[4] = {2, 3, 0, 1};
    constexpr unsigned map_z[4] = {1, 0, 3, 2};
    constexpr float sign_x[4] = {1, -1, 1, -1};
    constexpr float sign_y[4] = {1, 1, -1, -1};
    constexpr float sign_z[4] = {-1, 1, 1, -1};
    for (uint32_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        auto& result = complete[i];
        for (unsigned lane = 0; lane < 4; ++lane) {
            const float delta = reflex_detail::mul(node.linear_velocity[lane], in.dt);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.position[lane] = reflex_detail::add(delta, node.position[lane]);
        }
        if (!finite_vector(result.position)) return REFLEX_NUMERIC_RANGE;
        const float omega[4] = {node.angular_velocity[0], node.angular_velocity[1],
                                node.angular_velocity[2], in.angular_w};
        const auto& q = node.orientation;
        for (unsigned lane = 0; lane < 4; ++lane) {
            // 85d16c..85d1d3: preserve shuffle, operand, and grouping order.
            // The sign vectors are multiplications, not sign-bit operations.
            const float tx = reflex_detail::mul(reflex_detail::mul(q[0], omega[map_x[lane]]), sign_x[lane]);
            const float ty = reflex_detail::mul(reflex_detail::mul(q[1], omega[map_y[lane]]), sign_y[lane]);
            const float tz = reflex_detail::mul(reflex_detail::mul(omega[map_z[lane]], q[2]), sign_z[lane]);
            const float tw = reflex_detail::mul(q[3], omega[lane]);
            if (!reflex_detail::finite(tx) || !reflex_detail::finite(ty)
                || !reflex_detail::finite(tz) || !reflex_detail::finite(tw)) return REFLEX_NUMERIC_RANGE;
            const float zy = reflex_detail::add(tz, ty);
            const float wx = reflex_detail::add(tw, tx);
            if (!reflex_detail::finite(zy) || !reflex_detail::finite(wx)) return REFLEX_NUMERIC_RANGE;
            const float derivative = reflex_detail::add(zy, wx);
            if (!reflex_detail::finite(derivative)) return REFLEX_NUMERIC_RANGE;
            const float delta = reflex_detail::mul(derivative, half_dt);
            if (!reflex_detail::finite(delta)) return REFLEX_NUMERIC_RANGE;
            result.orientation[lane] = reflex_detail::add(delta, q[lane]);
        }
        if (!finite_vector(result.orientation)) return REFLEX_NUMERIC_RANGE;
    }

    // Native advances every node before the normalization pass. Bounded scratch
    // additionally makes this API's finite-domain failures atomic.
    for (uint32_t i = 0; i < count; ++i) {
        auto& q = complete[i].orientation;
        float sq[4];
        for (unsigned lane = 0; lane < 4; ++lane) sq[lane] = reflex_detail::mul(q[lane], q[lane]);
        if (!finite_vector(sq)) return REFLEX_NUMERIC_RANGE;
        const float zx = reflex_detail::add(sq[2], sq[0]);
        const float wy = reflex_detail::add(sq[3], sq[1]);
        if (!reflex_detail::finite(zx) || !reflex_detail::finite(wy)) return REFLEX_NUMERIC_RANGE;
        const float norm2 = reflex_detail::add(wy, zx);
        if (!reflex_detail::finite(norm2)) return REFLEX_NUMERIC_RANGE;
        const float norm = reflex_detail::sqrt(norm2);
        if (norm == 0.0f) {
            // SSE divides before masking. Only output bits, not sticky exception
            // status, are part of this API's agreement for a zero norm.
            for (float& lane : q) lane = 0.0f;
        } else {
            for (float& lane : q) lane = reflex_detail::div(lane, norm);
            if (!finite_vector(q)) return REFLEX_NUMERIC_RANGE;
        }
    }
    std::memcpy(output, complete, output_size);
    return REFLEX_OK;
}

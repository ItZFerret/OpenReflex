#include "reflex_core.h"

#include "fp32.h"

#include <cmath>
#include <cstring>

static_assert(sizeof(reflex_scheduler_input) == 32, "ABI layout changed");
static_assert(sizeof(reflex_scheduler_plan) == 20, "ABI layout changed");
static_assert(sizeof(reflex_interpolation_input) == 8, "ABI layout changed");
static_assert(sizeof(reflex_interpolation_weights) == 8, "ABI layout changed");
static_assert(alignof(reflex_scheduler_input) == 4 && alignof(reflex_scheduler_plan) == 4
              && alignof(reflex_interpolation_input) == 4 && alignof(reflex_interpolation_weights) == 4,
              "ABI alignment changed");

using namespace reflex_detail;

uint32_t REFLEX_CALL reflex_core_abi_version(void) { return REFLEX_CORE_ABI_VERSION; }

uint32_t REFLEX_CALL reflex_core_sizeof(uint32_t id) {
    switch (id) {
        case REFLEX_SCHEDULER_INPUT: return sizeof(reflex_scheduler_input);
        case REFLEX_SCHEDULER_PLAN: return sizeof(reflex_scheduler_plan);
        case REFLEX_INTERPOLATION_INPUT: return sizeof(reflex_interpolation_input);
        case REFLEX_INTERPOLATION_WEIGHTS: return sizeof(reflex_interpolation_weights);
        case REFLEX_VELOCITY_INPUT: return sizeof(reflex_velocity_input);
        case REFLEX_VELOCITY_NODE_INPUT: return sizeof(reflex_velocity_node_input);
        case REFLEX_VELOCITY_NODE_OUTPUT: return sizeof(reflex_velocity_node_output);
        case REFLEX_POSE_INPUT: return sizeof(reflex_pose_input);
        case REFLEX_POSE_NODE_INPUT: return sizeof(reflex_pose_node_input);
        case REFLEX_POSE_NODE_OUTPUT: return sizeof(reflex_pose_node_output);
        case REFLEX_FORCE_INPUT: return sizeof(reflex_force_input);
        case REFLEX_FORCE_NODE_INPUT: return sizeof(reflex_force_node_input);
        case REFLEX_FORCE_NODE_OUTPUT: return sizeof(reflex_force_node_output);
        case REFLEX_MATRIX_INPUT: return sizeof(reflex_matrix_input);
        case REFLEX_MATRIX_OUTPUT: return sizeof(reflex_matrix_output);
        case REFLEX_MATRIX4_MULTIPLY_INPUT: return sizeof(reflex_matrix4_multiply_input);
        case REFLEX_MATRIX4_OUTPUT: return sizeof(reflex_matrix4_output);
        case REFLEX_MATRIX6_MULTIPLY_INPUT: return sizeof(reflex_matrix6_multiply_input);
        case REFLEX_MATRIX4X6_SUBTRACT_INPUT: return sizeof(reflex_matrix4x6_subtract_input);
        case REFLEX_MATRIX6X4_SUBTRACT_INPUT: return sizeof(reflex_matrix6x4_subtract_input);
        case REFLEX_FIXED_SLOT3_INPUT: return sizeof(reflex_fixed_slot3_input);
    case REFLEX_ROTOR_SLOT3_INPUT: return sizeof(reflex_rotor_slot3_input);
    case REFLEX_FIXED_SLOT5_INPUT: return sizeof(reflex_fixed_slot5_input);
    case REFLEX_ROTOR_SLOT5_INPUT: return sizeof(reflex_rotor_slot5_input);
    case REFLEX_BODY_FORCE_OUTPUT: return sizeof(reflex_body_force_output);
    case REFLEX_INVERSE3_INPUT: return sizeof(reflex_inverse3_input);
    case REFLEX_INVERSE3_OUTPUT: return sizeof(reflex_inverse3_output);
    case REFLEX_MATRIX3_PRODUCT_INPUT: return sizeof(reflex_matrix3_product_input);
    case REFLEX_INVERSE6_INPUT: return sizeof(reflex_inverse6_input);
    case REFLEX_INVERSE6_OUTPUT: return sizeof(reflex_inverse6_output);
    case REFLEX_INVERSE4_INPUT: return sizeof(reflex_inverse4_input);
    case REFLEX_INVERSE4_OUTPUT: return sizeof(reflex_inverse4_output);
    case REFLEX_BIKE_FORCE_STAGE_INPUT: return sizeof(reflex_bike_force_stage_input);
    case REFLEX_BIKE_FORCE_STAGE_OUTPUT: return sizeof(reflex_bike_force_stage_output);
    case REFLEX_MECHANISM_CORE_INPUT: return sizeof(reflex_mechanism_core_input);
    case REFLEX_MECHANISM_NODE_INPUT: return sizeof(reflex_mechanism_node_input);
    case REFLEX_MECHANISM_NODE_OUTPUT: return sizeof(reflex_mechanism_node_output);
    case REFLEX_PREPARATION_INPUT: return sizeof(reflex_preparation_input);
    case REFLEX_PREPARATION_NODE_INPUT: return sizeof(reflex_preparation_node_input);
    case REFLEX_PREPARATION_NODE_OUTPUT: return sizeof(reflex_preparation_node_output);
    case REFLEX_CONSTRAINT_SOLVE_INPUT: return sizeof(reflex_constraint_solve_input);
    case REFLEX_CONSTRAINT_NODE_INPUT: return sizeof(reflex_constraint_node_input);
    case REFLEX_CONSTRAINT_NODE_OUTPUT: return sizeof(reflex_constraint_node_output);
    case REFLEX_ROTOR_PRODUCT4_INPUT: return sizeof(reflex_rotor_product4_input);
    case REFLEX_ROTOR_PRODUCT4_OUTPUT: return sizeof(reflex_rotor_product4_output);
    default: return 0;
    }
}

uint32_t REFLEX_CALL reflex_scheduler_evaluate(
    const reflex_scheduler_input *input, uint32_t input_size,
    reflex_scheduler_plan *output, uint32_t output_size) {
    if (!input || !output) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input) || output_size != sizeof(*output)) return REFLEX_ABI_MISMATCH;

    const reflex_scheduler_input in = *input;
    reflex_scheduler_plan result{};
    if (in.simulation_type == 1 && in.flag_237c == 0) {
        // 430f10 returns before timing arithmetic, writes or interpolation.
        result.branch = REFLEX_SCHEDULE_EARLY_RETURN;
        result.scheduled_steps = in.previous_scheduled_steps;
        std::memcpy(&result.lead, &in.lead, sizeof(in.lead));
        *output = result;
        return REFLEX_OK;
    }

    if (!supported_fp()) return REFLEX_UNSUPPORTED_FP;
    if (!finite(in.frame_dt) || !finite(in.lead) || !finite(in.fixed_dt)
        || in.fixed_dt <= 0.0f || in.max_steps < 0) return REFLEX_INVALID_INPUT;

    const float deficit = sub(in.frame_dt, in.lead); // 430f36
    if (!finite(deficit)) return REFLEX_NUMERIC_RANGE;
    result.flags = REFLEX_SCHEDULE_WRITES_STATE | REFLEX_SCHEDULE_INTERPOLATES;
    if (deficit <= 0.0f) {
        result.branch = REFLEX_SCHEDULE_INTERPOLATION_ONLY;
        result.lead = sub(in.lead, in.frame_dt);
    } else {
        result.branch = REFLEX_SCHEDULE_STEPS;
        const float quotient = div(deficit, in.fixed_dt); // 4314e9
        if (!finite(quotient)) return REFLEX_NUMERIC_RANGE;
        // Widen only AFTER binary32 division, exactly like CVTSS2SD -> ceil.
        const double rounded = std::ceil(static_cast<double>(quotient));
        if (rounded < 0.0 || rounded > 2147483647.0) return REFLEX_NUMERIC_RANGE;
        const int32_t requested = static_cast<int32_t>(rounded);
        result.scheduled_steps = requested;
        if (requested > in.max_steps) {
            result.scheduled_steps = in.max_steps;
            result.flags |= REFLEX_SCHEDULE_CAPPED;
            result.lead = 0.0f; // XORPS: always positive zero.
        } else {
            const float product = mul(from_int(requested), in.fixed_dt);
            if (!finite(product)) return REFLEX_NUMERIC_RANGE;
            result.lead = sub(product, deficit);
        }
        if (in.mode_2de4 != 3 && in.mode_2de4 != 4)
            result.planned_integrations = result.scheduled_steps;
    }
    if (!finite(result.lead)) return REFLEX_NUMERIC_RANGE;
    *output = result;
    return REFLEX_OK;
}

uint32_t REFLEX_CALL reflex_scheduler_interpolation(
    const reflex_interpolation_input *input, uint32_t input_size,
    reflex_interpolation_weights *output, uint32_t output_size) {
    if (!input || !output) return REFLEX_NULL_ARGUMENT;
    if (input_size != sizeof(*input) || output_size != sizeof(*output)) return REFLEX_ABI_MISMATCH;
    if (!supported_fp()) return REFLEX_UNSUPPORTED_FP;
    const reflex_interpolation_input in = *input;
    if (!finite(in.lead) || !finite(in.fixed_dt) || in.fixed_dt <= 0.0f) return REFLEX_INVALID_INPUT;
    const float ratio = div(in.lead, in.fixed_dt);
    if (!finite(ratio)) return REFLEX_NUMERIC_RANGE;
    reflex_interpolation_weights complete{sub(1.0f, ratio), 0.0f};
    complete.previous = sub(1.0f, complete.current);
    if (!finite(complete.current) || !finite(complete.previous)) return REFLEX_NUMERIC_RANGE;
    *output = complete;
    return REFLEX_OK;
}

#ifndef REFLEX_CORE_H
#define REFLEX_CORE_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(REFLEX_CORE_BUILD)
#    define REFLEX_API __declspec(dllexport)
#  else
#    define REFLEX_API __declspec(dllimport)
#  endif
#  define REFLEX_CALL __cdecl
#else
#  define REFLEX_API __attribute__((visibility("default")))
#  define REFLEX_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define REFLEX_CORE_ABI_VERSION 1u

enum reflex_status {
    REFLEX_OK = 0,
    REFLEX_NULL_ARGUMENT = 1,
    REFLEX_ABI_MISMATCH = 2,
    REFLEX_INVALID_INPUT = 3,
    REFLEX_UNSUPPORTED_FP = 4,
    REFLEX_NUMERIC_RANGE = 5
};

enum reflex_struct_id {
    REFLEX_SCHEDULER_INPUT = 1,
    REFLEX_SCHEDULER_PLAN = 2,
    REFLEX_INTERPOLATION_INPUT = 3,
    REFLEX_INTERPOLATION_WEIGHTS = 4,
    REFLEX_VELOCITY_INPUT = 5,
    REFLEX_VELOCITY_NODE_INPUT = 6,
    REFLEX_VELOCITY_NODE_OUTPUT = 7,
    REFLEX_POSE_INPUT = 8,
    REFLEX_POSE_NODE_INPUT = 9,
    REFLEX_POSE_NODE_OUTPUT = 10,
    REFLEX_FORCE_INPUT = 11,
    REFLEX_FORCE_NODE_INPUT = 12,
    REFLEX_FORCE_NODE_OUTPUT = 13,
    REFLEX_MATRIX_INPUT = 14,
    REFLEX_MATRIX_OUTPUT = 15,
    REFLEX_MATRIX4_MULTIPLY_INPUT = 16,
    REFLEX_MATRIX4_OUTPUT = 17,
    REFLEX_MATRIX6_MULTIPLY_INPUT = 18,
    REFLEX_MATRIX4X6_SUBTRACT_INPUT = 19,
    REFLEX_MATRIX6X4_SUBTRACT_INPUT = 20,
    REFLEX_FIXED_SLOT3_INPUT = 21,
    REFLEX_ROTOR_SLOT3_INPUT = 22,
    REFLEX_FIXED_SLOT5_INPUT = 23,
    REFLEX_ROTOR_SLOT5_INPUT = 24,
    REFLEX_BODY_FORCE_OUTPUT = 25,
    REFLEX_INVERSE3_INPUT = 26,
    REFLEX_INVERSE3_OUTPUT = 27,
    REFLEX_MATRIX3_PRODUCT_INPUT = 28,
    REFLEX_INVERSE6_INPUT = 29,
    REFLEX_INVERSE6_OUTPUT = 30,
    REFLEX_INVERSE4_INPUT = 31,
    REFLEX_INVERSE4_OUTPUT = 32,
    REFLEX_ROTOR_PRODUCT4_INPUT = 33,
    REFLEX_ROTOR_PRODUCT4_OUTPUT = 34,
    REFLEX_CONSTRAINT_SOLVE_INPUT = 35,
    REFLEX_CONSTRAINT_NODE_INPUT = 36,
    REFLEX_CONSTRAINT_NODE_OUTPUT = 37,
    REFLEX_PREPARATION_INPUT = 38,
    REFLEX_PREPARATION_NODE_INPUT = 39,
    REFLEX_PREPARATION_NODE_OUTPUT = 40,
    REFLEX_MECHANISM_CORE_INPUT = 41,
    REFLEX_MECHANISM_NODE_INPUT = 42,
    REFLEX_MECHANISM_NODE_OUTPUT = 43,
    REFLEX_BIKE_FORCE_STAGE_INPUT = 44,
    REFLEX_BIKE_FORCE_STAGE_OUTPUT = 45
};

/* Original0043e100 complete bike force stage over one supplied raw state.
 * Embedded pointer words are opaque and never dereferenced. All fields are
 * raw little-endian binary32/integer words; no full-state finite scan is made.
 * Runtime x87 controls007f/027f and conversion flag0/1 are modeled explicitly.
 */
typedef struct reflex_bike_force_stage_input {
    uint32_t state_bits[9664];
    uint32_t control_bits[64];
    uint32_t axis_x_bits[4],axis_y_bits[4],numerator_bits[4],g_bits[4];
    uint32_t dt_bits,conversion_flag,x87_control_word;
} reflex_bike_force_stage_input;
typedef struct reflex_bike_force_stage_output {
    uint32_t state_bits[9664];
} reflex_bike_force_stage_output;

/* Exact sizes,4-byte alignment and nonwrapping spans; overlapping output is
 * supported by a full snapshot. Every failure is atomic; host FP controls
 * and sticky status are restored. No world query/contact generation occurs.
 * Handwritten stages retain finite-input/intermediate checks. The bounded
 * original tire instruction interpreter preserves discarded packed NaNs and
 * masks, but rejects nonfinite final written floats, bad indices/configuration,
 * out-of-map accesses and unsupported operations. Unchanged raw bytes remain
 * opaque. Engine fourth-root pow parity is verified on recorded original/live
 * ranges, not an implementation of the complete VC90 mathematical runtime.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_bike_force_stage(
    const reflex_bike_force_stage_input *input,uint32_t input_size,
    reflex_bike_force_stage_output *output,uint32_t output_size);

enum reflex_schedule_branch {
    REFLEX_SCHEDULE_EARLY_RETURN = 0,
    REFLEX_SCHEDULE_INTERPOLATION_ONLY = 1,
    REFLEX_SCHEDULE_STEPS = 2
};

enum reflex_schedule_flags {
    REFLEX_SCHEDULE_WRITES_STATE = 1u,
    REFLEX_SCHEDULE_INTERPOLATES = 2u,
    REFLEX_SCHEDULE_CAPPED = 4u
};

/* Inputs to the arithmetic/control decisions in original function 00430ef0.
 * The separate 00430eb0 enabled gate is the caller's responsibility.
 * Values use native seconds. Every float is IEEE-754 binary32.
 */
typedef struct reflex_scheduler_input {
    float frame_dt;
    float lead;
    float fixed_dt;
    int32_t max_steps;
    int32_t previous_scheduled_steps;
    int32_t simulation_type;
    uint32_t flag_237c;
    int32_t mode_2de4;
} reflex_scheduler_input;

typedef struct reflex_scheduler_plan {
    uint32_t branch;
    uint32_t flags;
    int32_t scheduled_steps;
    /* Projection assuming callbacks leave mode and count unchanged. Native
     * slot 3 runs before the mode check; loop callbacks can change the count.
     * This API does not run those callbacks, integrate bodies or interpolate poses.
     */
    int32_t planned_integrations;
    float lead;
} reflex_scheduler_plan;

/* The original computes weights AFTER callbacks/integration. Pass the current
 * post-callback values, not necessarily the initial plan's values. No clamping.
 */
typedef struct reflex_interpolation_input {
    float lead;
    float fixed_dt;
} reflex_interpolation_input;

typedef struct reflex_interpolation_weights {
    float current;
    float previous;
} reflex_interpolation_weights;

/* Additive ABI-1 extension for original 0085d270. Ten is the verified
 * mechanism allocation capacity, NOT a clamp performed by the original loop.
 */
#define REFLEX_VELOCITY_MAX_NODES 10u

typedef struct reflex_velocity_input {
    float dt;
    int32_t node_count;
} reflex_velocity_input;

/* All four vector lanes participate, including W. Coefficients and resolved
 * forces are inputs from the surrounding native pipeline; this API does not
 * calculate forces, constraints, or inertia/frame conversions.
 */
typedef struct reflex_velocity_node_input {
    float linear_velocity[4];
    float reciprocal_mass[4];
    float linear_force[4];
    float angular_velocity[4];
    float angular_coefficient[4];
    float angular_force[4];
} reflex_velocity_node_input;

typedef struct reflex_velocity_node_output {
    float linear_accel[4];
    float angular_accel[4];
    float linear_velocity[4];
    float angular_velocity[4];
} reflex_velocity_node_output;

/* Additive ABI-1 extension for original 0085d100. The global angular W at
 * original 00d6166c replaces each node's stored angular_velocity[3]. */
#define REFLEX_POSE_MAX_NODES 10u

typedef struct reflex_pose_input {
    float dt;
    int32_t node_count;
    float angular_w;
} reflex_pose_input;

typedef struct reflex_pose_node_input {
    float position[4];
    float linear_velocity[4];
    float orientation[4]; /* Quaternion x, y, z, w. */
    float angular_velocity[4];
} reflex_pose_node_input;

typedef struct reflex_pose_node_output {
    float position[4];
    float orientation[4];
} reflex_pose_node_output;

/* Additive ABI-1 extension for resolved-force assembly 0085ca40. */
#define REFLEX_FORCE_MAX_NODES 10u

typedef struct reflex_force_input {
    float dt;
    int32_t node_count;
    float gravity[4];
    float matrix_global[4]; /* Original live global vec4 at 00d61670. */
} reflex_force_input;

typedef struct reflex_force_node_input {
    float mass[4];
    float accumulated_linear_force[4];
    float linear_impulse[4];
    float angular_velocity[4];
    float angular_coefficient[4]; /* M+680, distinct from velocity's M+690. */
    float accumulated_angular_force[4];
    float angular_impulse[4];
} reflex_force_node_input;

typedef struct reflex_force_node_output {
    float linear_force[4];
    float angular_force[4];
} reflex_force_node_output;

/* Before-call value snapshots for original four-block leaf 00857b30. Rows
 * are grouped as four consecutive 3-row blocks in native address order:
 * A+20/30/40, +60/70/80, +a0/b0/c0, +e0/f0/100. Source/destination vec4s
 * correspond to X/Y+10 and +20. Native object headers are not arithmetic.
 */
typedef struct reflex_matrix_input {
    float rows[12][4];
    float source[2][4];
    float destination[2][4];
} reflex_matrix_input;

typedef struct reflex_matrix_output {
    float destination[2][4];
} reflex_matrix_output;

/* Additive inputs for the other four observed matrix leaves. Every row and
 * source preserves its native vec4 lanes. Rows are in increasing native
 * address order with object/block headers removed. Multiply inputs omit old
 * destination bytes because the original overwrite leaves never read them.
 */
typedef struct reflex_matrix4_multiply_input {
    float rows[4][4]; /* A+10,20,30,40. */
    float source[4];
} reflex_matrix4_multiply_input;

typedef struct reflex_matrix4_output {
    float destination[4];
} reflex_matrix4_output;

typedef struct reflex_matrix6_multiply_input {
    float rows[12][4]; /* Same row offsets as reflex_matrix_input. */
    float source[2][4];
} reflex_matrix6_multiply_input;

typedef struct reflex_matrix4x6_subtract_input {
    float rows[8][4]; /* A+20,30,40,50,70,80,90,a0. */
    float source[2][4];
    float destination[4];
} reflex_matrix4x6_subtract_input;

typedef struct reflex_matrix6x4_subtract_input {
    float rows[6][4]; /* A+20,30,40,60,70,80. */
    float source[4];
    float destination[2][4];
} reflex_matrix6x4_subtract_input;

REFLEX_API uint32_t REFLEX_CALL reflex_core_abi_version(void);
REFLEX_API uint32_t REFLEX_CALL reflex_core_sizeof(uint32_t struct_id);

/* Return a reflex_status. On failure output is untouched. Exact struct sizes
 * are required; check the ABI version and sizes before using from a binding.
 * Arithmetic requires nearest-even rounding, gradual underflow, masked FP
 * exceptions, and finite inputs with fixed_dt > 0 and max_steps >= 0.
 * Unsupported caller FP controls are rejected, never silently changed.
 * The early-return guard runs before those arithmetic checks, preserving the
 * previous count and lead bits even when unused timing fields are invalid.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_scheduler_evaluate(
    const reflex_scheduler_input *input, uint32_t input_size,
    reflex_scheduler_plan *output, uint32_t output_size);

REFLEX_API uint32_t REFLEX_CALL reflex_scheduler_interpolation(
    const reflex_interpolation_input *input, uint32_t input_size,
    reflex_interpolation_weights *output, uint32_t output_size);

/* Apply the velocity arithmetic to 0..10 nodes. Array byte sizes must equal
 * node_count * sizeof(element); input_size must equal sizeof(*input).
 * Nonempty calls require nonnull, nonoverlapping output/input buffers, finite
 * dt and all six input vectors, and the same FP controls as the scheduler.
 * Buffers must meet their declared structs' four-byte alignment requirement.
 * Negative dt and signed zero are valid. Overflow of ANY intermediate or
 * output rejects the entire call; every output byte remains untouched on
 * failure. Inputs and caller FP controls are never changed; FP sticky status
 * flags may change during arithmetic. Separate multiply/add round to float32.
 * Zero nodes returns OK without arithmetic, FP checks, or inspecting dt;
 * null node/output arrays are valid with zero byte sizes. Negative node counts
 * are rejected, although the original signed loop would perform no work.
 * NaN/Infinity propagation and exception-flag equivalence are outside scope.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_velocity_integrate(
    const reflex_velocity_input *input, uint32_t input_size,
    const reflex_velocity_node_input *nodes, uint32_t nodes_size,
    reflex_velocity_node_output *output, uint32_t output_size);

/* Advance positions and orientations, then normalize each orientation with
 * the original separately rounded binary32 operation order. The velocity
 * API's array sizes, alignment, nonoverlap, FP control, and atomic failure
 * requirements also apply here. Every input lane must be finite, including
 * the unused stored angular W; negative dt and signed zero are supported.
 * Zero nodes skips arithmetic/FP checks and ignores dt and angular_w.
 * A zero rounded quaternion norm returns four +0 words. Exception status
 * equivalence is outside scope (native executes division before zero masking).
 * Overflow of a contributing intermediate, including norm squared, rejects
 * the entire call. Native infinite norm squared instead returns canonical
 * NaNs; that behavior and general NaN/Infinity propagation are outside scope.
 * This API does not execute callbacks or recompute velocity/constraints.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_pose_integrate(
    const reflex_pose_input *input, uint32_t input_size,
    const reflex_pose_node_input *nodes, uint32_t nodes_size,
    reflex_pose_node_output *output, uint32_t output_size);

/* Compose resolved forces from supplied accumulated forces, impulses, mass,
 * angular coefficients and velocities. All input words must be finite and a
 * nonempty call requires dt != 0; negative dt is valid. The native reciprocal
 * timestep, matrix helper and dot reductions each round separately to float32.
 * The angular W correction duplicates the Z correction, not zero.
 * Existing velocity/pose size, alignment, nonoverlap, FP controls and atomic
 * failure contracts apply. Contributing intermediate overflow rejects the
 * entire call. The helper's discarded momentum/product W lanes are not
 * computed here; their overflow, NaNs and exception flags are outside scope.
 * Zero nodes skips the native pre-loop reciprocal and all FP/input checks;
 * null node/output arrays with zero byte sizes are valid. General nonfinite
 * propagation and exception-status equivalence remain outside scope.
 * This API does not generate the accumulated inputs or execute constraints.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_force_compose(
    const reflex_force_input *input, uint32_t input_size,
    const reflex_force_node_input *nodes, uint32_t nodes_size,
    reflex_force_node_output *output, uint32_t output_size);

/* Concrete callback snapshots. Matrix rows retain their native block order.
 * Slot 3: two matrices, four force/coefficient pairs, then RHS bias.
 * Slot 5: two matrices, solved vector, then four initial body forces.
 * aliases[i] is the first index of body i's force destination (0<=alias<=i,
 * aliases[alias]==alias). Equal destinations require identical initial words.
 */
typedef struct reflex_fixed_slot3_input {
    float matrices[2][12][4]; float forces[4][4];
    float coefficients[4][4]; float bias[2][4];
} reflex_fixed_slot3_input;
typedef struct reflex_rotor_slot3_input {
    float matrices[2][8][4]; float forces[4][4];
    float coefficients[4][4]; float bias[1][4];
} reflex_rotor_slot3_input;
typedef struct reflex_fixed_slot5_input {
    float matrices[2][12][4]; float solution[2][4];
    float forces[4][4]; uint32_t aliases[4];
} reflex_fixed_slot5_input;
typedef struct reflex_rotor_slot5_input {
    float matrices[2][8][4]; float solution[1][4];
    float forces[4][4]; uint32_t aliases[4];
} reflex_rotor_slot5_input;
typedef struct reflex_body_force_output { float forces[4][4]; } reflex_body_force_output;

/* Original 0085bb00/0085a7e0 and 0085c0d0/0085b5a0 arithmetic.
 * Same snapshot/overlap, exact size/alignment, FP and atomic error contracts
 * as the matrix leaves. All supplied floats finite; contributing intermediate
 * overflow rejects the entire call. Discarded W products/packed additions and
 * FP exception flags are outside scope. Slot 3 computes (parent+own)+bias;
 * slot 5 transposes captured matrices and applies forces in body index order,
 * including duplicate destination dependencies. All four returned forces are
 * final values, so aliased outputs agree. This does not prepare matrices,
 * generate constraints, invoke a solver, or dereference game addresses.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_fixed_slot3(
    const reflex_fixed_slot3_input*, uint32_t, reflex_matrix_output*, uint32_t);
REFLEX_API uint32_t REFLEX_CALL reflex_rotor_slot3(
    const reflex_rotor_slot3_input*, uint32_t, reflex_matrix4_output*, uint32_t);
REFLEX_API uint32_t REFLEX_CALL reflex_fixed_slot5(
    const reflex_fixed_slot5_input*, uint32_t, reflex_body_force_output*, uint32_t);
REFLEX_API uint32_t REFLEX_CALL reflex_rotor_slot5(
    const reflex_rotor_slot5_input*, uint32_t, reflex_body_force_output*, uint32_t);

/* Original 00859250 with verified Matrix3x3 transpose 00857c90.
 * Rows retain all four stored lanes. Numerator is the captured runtime vec4,
 * not implicitly one. Mask must be {0xffffffff,0xffffffff,0xffffffff,0},
 * transpose_rva must be 0x00457c90 (identity only; never an invoked address).
 */
typedef struct reflex_inverse3_input {
    float rows[3][4]; float numerator[4]; uint32_t mask[4]; uint32_t transpose_rva;
} reflex_inverse3_input;
typedef struct reflex_inverse3_output { float rows[3][4]; } reflex_inverse3_output;

/* Separate binary32 cofactor products/subtractions, determinant ((x+y)+z),
 * per-lane division and scaled cofactors, then the original transpose.
 * Output rows0/1 W duplicate Z. Row2 W is +0 * (numerator.W/determinant),
 * retaining its sign. No pivoting or determinant tolerance is introduced.
 * Same exact-size/alignment, snapshot overlap, supported FP and atomic-error
 * contracts as matrix leaves. Every supplied float must be finite. Zero
 * determinant or nonfinite contributing intermediate/scale/output returns
 * NUMERIC_RANGE with no output write. Unsupported mask/target is INVALID_INPUT.
 * Masked cofactor-W arithmetic and FP exception status are outside scope.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_matrix3_inverse(
    const reflex_inverse3_input*, uint32_t, reflex_inverse3_output*, uint32_t);

/* Original 00853740. P is the ECX right operand, Q the EDI left operand;
 * output = Q * P. The transpose identity must be 0x00457c90. All stored
 * input lanes must be finite, including unused W; output W always equals Z.
 * Every XYZ product and ((x+y)+z) reduction rounds separately. Discarded W
 * products and their exception effects are outside the value contract.
 */
typedef struct reflex_matrix3_product_input {
    float p[3][4]; float q[3][4]; uint32_t p_transpose_rva;
} reflex_matrix3_product_input;
REFLEX_API uint32_t REFLEX_CALL reflex_matrix3_product(
    const reflex_matrix3_product_input*, uint32_t, reflex_inverse3_output*, uint32_t);

/* Original 008597e0 ordered composition, blocks A,B,C,D in row-major order.
 * Runtime numerator/mask retain the inverse3 contract. Target identities are
 * respectively 457c90,457c90,457bf0,4581e0; they are never invoked addresses.
 * E=inv(A), T=C*E, U=E*B, V=C*U, K=V-D, H=inv(K), B'=U*H,
 * C'=H*T, R=U*C', A'=E-R, D'=+0-H; subtraction retains all four lanes.
 * Both inverses must meet the finite component domain, including K.W.
 * No pivoting or singular fallback is supplied. All 48 output words matter:
 * A.row2.W may differ from Z; D.row2.W is +0 under the supported FP profile.
 * Both APIs use exact-size/four-byte alignment, input snapshots and atomic
 * output commit, supporting every valid aligned input/output overlap. All
 * nonfinite contributing intermediates return NUMERIC_RANGE. No full matrix
 * preparation, inverse4 embedding or complete constraint-solve is claimed.
 */
typedef struct reflex_inverse6_input {
    float blocks[4][3][4]; float numerator[4]; uint32_t mask[4];
    uint32_t source_b_transpose_rva, destination_c_transpose_rva;
    uint32_t destination_d_copy_rva, destination_d_negate_rva;
} reflex_inverse6_input;
typedef struct reflex_inverse6_output { float blocks[4][3][4]; } reflex_inverse6_output;
REFLEX_API uint32_t REFLEX_CALL reflex_matrix6_inverse(
    const reflex_inverse6_input*, uint32_t, reflex_inverse6_output*, uint32_t);

/* Original 00859340: bit-preserving 4x4 embedding into the verified inverse6,
 * followed by sixteen exact result-word selections. Globals are captured
 * runtime vectors, not hard-coded identity values. All supplied float lanes
 * must be finite, including axis_x.X overwritten during embedding. Mask is
 * the existing XYZ mask. Local concrete virtual targets are constructor-fixed.
 * Inherits inverse6 finite-intermediate (including K.W) and singular-leading-
 * block/Schur contracts; no pivot or generic 4x4 fallback is introduced.
 * Exact sizes/four-byte alignment and supported FP are required. Snapshot
 * input and atomic output support every valid aligned input/output overlap;
 * errors publish no output bytes. FP exception equivalence is outside scope.
 */
typedef struct reflex_inverse4_input {
    float rows[4][4]; float zero[4], axis_x[4], axis_y[4], axis_z[4], numerator[4];
    uint32_t mask[4];
} reflex_inverse4_input;
typedef struct reflex_inverse4_output { float rows[4][4]; } reflex_inverse4_output;
REFLEX_API uint32_t REFLEX_CALL reflex_matrix4_inverse(
    const reflex_inverse4_input*, uint32_t, reflex_inverse4_output*, uint32_t);

/* Original 00856b40 on pairwise-disjoint original A/B/C objects. A has four
 * vec4 rows; old C has six. Old B is never read and is absent from this API.
 * C' = C * transpose(A), each four-lane dot separately rounded as
 * (pY+pW)+(pX+pZ). B contains transpose(C') in two four-row XYZ blocks,
 * with every B.W = B.X. All 56 output words are meaningful stored values.
 * All supplied float lanes and contributing intermediates must be finite.
 * Unselected original SIMD lane exception effects are outside value scope.
 * Exact sizes, four-byte alignment and supported FP controls are required.
 * This normalized API snapshots input and atomically publishes both results;
 * every valid aligned input/output overlap is supported. This does not admit
 * overlapping original A/B/C objects, whose native load/store order differs.
 */
typedef struct reflex_rotor_product4_input { float a_rows[4][4], c_rows[6][4]; } reflex_rotor_product4_input;
typedef struct reflex_rotor_product4_output { float b_blocks[2][4][4], c_rows[6][4]; } reflex_rotor_product4_output;
REFLEX_API uint32_t REFLEX_CALL reflex_rotor_product4(
    const reflex_rotor_product4_input*, uint32_t, reflex_rotor_product4_output*, uint32_t);

/* Literal original 0085c700 -> 0085cf80 interval for a rooted 1..10-node
 * graph. Prepared matrices and resolved initial forces are caller inputs;
 * this does not prepare matrices, generate forces, or integrate motion.
 * Numeric node-index order and stored child order are preserved. Parent
 * indices need not precede children. Slot-5 writes update shared parent
 * forces immediately before the next ascending node.
 * Matrix slots are original fields 38,3c,48,4c,50,58,5c,60. Each has capacity
 * 12 vec4 rows; root uses slot2 only. Fixed uses all 12 rows in every slot;
 * rotor row counts are 8,8,12,6,8,4,8,6. Unused capacity is ignored.
 * Coefficients/forces are this node's linear and angular body vectors.
 * Initial scratch is supplied because nonmonotonic graphs can read child68
 * before its reverse pass. Overwritten/unused values need not be finite;
 * inputs consumed by a numerical component and its contributing intermediate
 * values must be finite, including that component's retained W lanes.
 * Root initial_rhs[0] and initial68[0] must agree bitwise (original alias).
 * Unused output rows retain initial bytes. Every node64 is filled with two
 * copies of runtime global_g, not hardcoded zeros. FP exception effects are
 * excluded. Existing strict FP controls and four-byte alignment are required.
 * Exact byte sizes, valid graph, and finite consumed values are checked.
 * Both input buffers are snapshotted before any output write. All valid
 * aligned overlaps are supported; failure leaves every output byte unchanged.
 * This normalized-buffer policy does not widen the separately validated
 * constructor-specific original-object alias domain.
 */
#define REFLEX_CONSTRAINT_MAX_NODES 10u
enum reflex_constraint_kind { REFLEX_CONSTRAINT_ROOT=0, REFLEX_CONSTRAINT_FIXED=1, REFLEX_CONSTRAINT_ROTOR=2 };
typedef struct reflex_constraint_solve_input { uint32_t node_count; float global_g[4]; } reflex_constraint_solve_input;
typedef struct reflex_constraint_node_input {
    uint32_t kind; int32_t parent; uint32_t child_count; uint32_t children[5];
    float matrices[8][12][4];
    float coefficients[2][4], forces[2][4], bias[2][4];
    float initial_rhs[2][4], initial64[2][4], initial68[2][4];
} reflex_constraint_node_input;
typedef struct reflex_constraint_node_output {
    float forces[2][4], rhs[2][4], workspace[2][4], solution[2][4];
} reflex_constraint_node_output;
REFLEX_API uint32_t REFLEX_CALL reflex_constraint_solve(
    const reflex_constraint_solve_input*, uint32_t,
    const reflex_constraint_node_input*, uint32_t,
    reflex_constraint_node_output*, uint32_t);

/* Complete original 0085cb70 preparation from one entry graph. The caller
 * supplies non-root matrices .38/.3c generated by the preceding node pass,
 * each node's two own coefficients, and the seven captured runtime globals.
 * No previously prepared output is consumed. Count is 1..10, with root0,
 * parent-before-child indexing, and at most five stored-order children.
 * kind uses reflex_constraint_kind. Matrix order is 38,3c,44,48,4c,50,54,58,5c,60.
 * Root outputs use only44/48 (12 rows). Fixed uses12 rows per matrix; rotor
 * row counts are8,8,12,12,6,8,4,4,8,6. Unused normalized capacity is zero.
 * Final node_products[0/1] represent original slot8/9 result buffers: fixed
 * uses12/12 rows, rotor12/4. Root has neither, so both normalized arrays are0.
 * All six supplied float globals, coefficients and meaningful non-root source
 * rows must be finite. Root source matrices and unused capacity are ignored.
 * All contributing intermediates and inherited inverse domains are checked;
 * FP exception status and discarded SIMD behavior remain outside scope.
 * Exact sizes, four-byte alignment, and supported FP controls are required.
 * Inputs are snapshotted and all outputs committed atomically. Every valid
 * aligned normalized input/output overlap is supported. Original object aliases
 * are separately restricted to constructor-derived disjoint storage.
 */
typedef struct reflex_preparation_input {
    uint32_t node_count;
    float axis_x[4],axis_y[4],axis_z[4],numerator[4],zero4[4],zero3[4];
    uint32_t mask[4];
} reflex_preparation_input;
typedef struct reflex_preparation_node_input {
    uint32_t kind; int32_t parent; uint32_t child_count,children[5];
    float coefficients[2][4],matrix38[12][4],matrix3c[12][4];
} reflex_preparation_node_input;
typedef struct reflex_preparation_node_output {
    float matrices[10][12][4],node_products[2][12][4];
} reflex_preparation_node_output;
REFLEX_API uint32_t REFLEX_CALL reflex_prepare_matrices(
    const reflex_preparation_input*, uint32_t,
    const reflex_preparation_node_input*, uint32_t,
    reflex_preparation_node_output*, uint32_t);

/* Single-start-state original mechanism core: ascending node2 generation,
 * complete preparation, resolved-force composition, both constraint passes,
 * velocity and acceleration. Corresponds to 0085c6d0 entry through 0085d270
 * exit. Does not include pose integration, external gameplay force/contact
 * generation or post-integration corrections. No intermediate result is input.
 * Count 1..10; kind uses reflex_constraint_kind; root0/parent-1; parents must
 * precede children; at most five stored-order children. Original numeric order
 * and immediate shared-parent force updates are retained.
 * Own bodies are linear then angular; six vec4s map original offsets
 * 00,10,20,30,50,60: pose, velocity, mass/coefficient, inverse coefficient,
 * accumulated force and impulse. Parent bodies are found through the graph.
 * Source .38/.3c each have12-row capacity; fixed node2 overwrites their rows,
 * rotor node2 preserves6 initial rows. Local capacity is3 vec4s: fixed uses1,
 * rotor3, root0. Root source/local capacities are ignored.
 * Output contains preparation matrices/products, final bias, RHS/workspace/
 * solution and resolved forces/accelerations/velocities. Root unused bias/RHS/
 * solution and unused normalized capacity are zero; this does not assert any
 * original write to root placeholder objects or opaque captured bytes.
 * Body pose, coefficients, accumulated inputs and raw headers/tails are outside
 * normalized outputs and remain caller-owned. Raw adapters preserve them.
 * Used finite input lanes and finite contributing intermediates are required,
 * including inherited inverse and finite nonzero dt domains. Unused/overwritten
 * inputs may contain arbitrary bits. FP sticky flags/discarded SIMD operations
 * are excluded. Exact sizes, four-byte alignment and supported FP required.
 * Both inputs are snapshotted before arithmetic; all normalized overlaps are
 * supported; every error leaves output unchanged. Original raw object aliases
 * remain separately bounded to constructor-consistent storage.
 */
typedef struct reflex_mechanism_core_input {
    uint32_t node_count; float dt,gravity[4];
    float axis_x[4],axis_y[4],axis_z[4],global_g[4];
    float sign_x[4],sign_y[4],sign_z[4],conjugate[4];
    uint32_t mask[4]; float one,two,negative_two,negative_zero;
    float numerator[4],extra[4];
} reflex_mechanism_core_input;
typedef struct reflex_mechanism_node_input {
    uint32_t kind; int32_t parent; uint32_t child_count,children[5];
    float bodies[2][6][4],sources[2][12][4],locals[3][4];
} reflex_mechanism_node_input;
typedef struct reflex_mechanism_node_output {
    reflex_preparation_node_output preparation;
    float bias[2][4],rhs[2][4],workspace[2][4],solution[2][4];
    float forces[2][4],accelerations[2][4],velocities[2][4];
} reflex_mechanism_node_output;
REFLEX_API uint32_t REFLEX_CALL reflex_mechanism_core(
    const reflex_mechanism_core_input*, uint32_t,
    const reflex_mechanism_node_input*, uint32_t,
    reflex_mechanism_node_output*, uint32_t);

/* Subtract the four-block matrix product from two existing destination
 * vec4s. This is the specific observed 00857b30 leaf, not a shape dispatcher,
 * matrix generator, or complete constraint solve; there is no timestep.
 * Dot products use ((x+y)+z), then duplicate dot Z into W. Each product,
 * addition and subtraction rounds separately to float32. All 64 supplied
 * float words must be finite; contributing overflow rejects the entire call.
 * Discarded row/source W products are not computed; their exception effects,
 * general nonfinite propagation, and FP status equivalence are outside scope.
 * The same supported FP controls as the other kernels are required. Exact
 * struct byte sizes and four-byte-aligned, nonnull buffers are required.
 * Input is snapshotted before arithmetic, so output may overlap input at any
 * four-byte-aligned offset within valid storage. No output bytes change on
 * failure. A successful overlapping write naturally replaces those input
 * bytes; all other input bytes and caller FP controls remain unchanged.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_matrix_subtract(
    const reflex_matrix_input *input, uint32_t input_size,
    reflex_matrix_output *output, uint32_t output_size);

/* The same exact-size, four-byte alignment, FP profile, finite contributing
 * intermediate and atomic/overlapping-output contracts as matrix_subtract.
 * Every supplied input float must be finite; overwritten output bytes are
 * never inspected. These are specific arithmetic leaves, not a dispatcher.
 * 00853bd0: four four-lane dots, each (py+pw)+(px+pz), all products rounded.
 * 008540a0: four three-row xyz blocks, summed in pairs; each output W = Z.
 * 00857750: two four-row xyz blocks, summed then subtracted from old Y.
 * 00857a00: six four-lane dots, subtracted in triples; correction W repeats
 *          the FIRST dot of its triple, so output W need not equal X or Z.
 * In four-lane dot leaves, unselected native SIMD additions contain extra
 * source-X sums; their overflow/exception effects are outside value scope.
 * In xyz leaves, discarded W products are likewise outside value scope.
 */
REFLEX_API uint32_t REFLEX_CALL reflex_matrix4_multiply(
    const reflex_matrix4_multiply_input *input, uint32_t input_size,
    reflex_matrix4_output *output, uint32_t output_size);

REFLEX_API uint32_t REFLEX_CALL reflex_matrix6_multiply(
    const reflex_matrix6_multiply_input *input, uint32_t input_size,
    reflex_matrix_output *output, uint32_t output_size);

REFLEX_API uint32_t REFLEX_CALL reflex_matrix4x6_subtract(
    const reflex_matrix4x6_subtract_input *input, uint32_t input_size,
    reflex_matrix4_output *output, uint32_t output_size);

REFLEX_API uint32_t REFLEX_CALL reflex_matrix6x4_subtract(
    const reflex_matrix6x4_subtract_input *input, uint32_t input_size,
    reflex_matrix_output *output, uint32_t output_size);

#ifdef __cplusplus
}
#endif
#endif

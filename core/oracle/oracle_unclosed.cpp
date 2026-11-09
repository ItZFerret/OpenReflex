/* oracle_unclosed.cpp -- ORACLE BUILD ONLY: link seams for product-only owners (their files are #ifndef RX_ORACLE or refuse the
 * oracle build) that the verification worlds do not reach (four-wheel families, zlib / wrapper lifecycle, guest hash,
 * terrain-height observers). Reaching one aborts with its name: nothing is computed or fabricated. Taken from the Moto comparisons
 * (builds/sol_career_single/national-moto/scored-function-comparison-revision/unclosed_boundaries.cpp), 2026-10-25. */
#include <cstdio>
#include <cstdlib>
#define UNREACHED(name) extern "C" [[noreturn]] void name(){std::fprintf(stderr,"RX_UNCLOSED_BOUNDARY %s (oracle build)\n",#name);std::abort();}
UNREACHED(rx_compression_leaf_reg)
UNREACHED(rx_guest_destroy_dependency)
UNREACHED(rx_wrapper_dependency)
UNREACHED(rx_wrapper_divide_error)
UNREACHED(rx_zlib_lifecycle_callback)
UNREACHED(rx_zlib_lifecycle_compare)
// the four-wheel families' finite dependency boundaries: compiled into the oracle with RX_ORACLE_FOURWHEEL (build_cli.sh, w-parity5),
// so the four-wheel compositions run (On / Verify) inside the original's world; without it they stay link seams
#ifndef RX_ORACLE_FOURWHEEL
UNREACHED(rx_body_probe_dependency)
UNREACHED(rx_buggy_controls_dependency)
UNREACHED(rx_buggy_engine_dependency)
UNREACHED(rx_buggy_engine_power_dependency)
UNREACHED(rx_family_response_curve_native)
UNREACHED(rx_family_suspension_dependency)
UNREACHED(rx_fourwheel_active_dependency)
UNREACHED(rx_fourwheel_contact_dependency)
UNREACHED(rx_fourwheel_contact_dispatch_dependency)
UNREACHED(rx_fourwheel_query_dependency)
UNREACHED(rx_fourwheel_rest_contact_dependency)
UNREACHED(rx_pending_allocation_fault)
UNREACHED(rx_pending_dependency)
UNREACHED(rx_query_lookup_frame)
UNREACHED(rx_terrain_height_dependency)
UNREACHED(rx_terrain_height_texture)
UNREACHED(rx_terrain_texture_exact)
UNREACHED(rx_truck_controls_dependency)
UNREACHED(rx_utv_contact_response_curve)
UNREACHED(rx_utv_controls_dependency)
UNREACHED(rx_utv_engine_dependency)
UNREACHED(rx_utv_engine_power_dependency)
UNREACHED(rx_utv_force_gear_curve)
#endif

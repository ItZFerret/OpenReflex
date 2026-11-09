/* rng_session.inc -- the game's random number streams (include/rx_rng.h; w-rng), included by rx_session.c. Copies only. */
#include "rx_rng.h"
#include "crt.h"
#include "build_water.h"

int rx_session_rng(struct rx_session* s, rx_rng_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_RNG_VERSION) return 1;
    rx_rng_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v; v.version = RX_RNG_VERSION; v.valid = 1;
    const rx_addr m = crt_ptd_of(CRT_THREAD_MAIN), l = crt_ptd_of(CRT_THREAD_LOADER);
    v.crt_main = m ? U32(m, 0x14) : 0; v.crt_loader = l ? U32(l, 0x14) : 0;
    v.game_seed = U32(0, 0x009cb860); v.game_count = U32(0, 0x009cb864);
    v.ocean_valid = (uint32_t)rx_water_ocean_seed(&v.ocean_seed, &v.ocean_draws);
    *out = v;
    return 0;
}

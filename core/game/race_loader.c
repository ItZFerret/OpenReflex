/* race_loader.c -- the per-frame part of the vehicle part loader (the riders' vehicles stream their part packages during the
 * race). (core/notes/race.md "Loader") */
#include "race.h"
#include "race_extern.h"
#include "build_rider.h"
#include "build_vehicle.h"

/* 0053ca70 VehicleLoader_PartPackagesReady (thiscall part, arg; RET 4) and 00543e50 PackageCache_Update (EAX = cache, arg = dt;
 * RET 4) are ported (build_rider / build_vehicle); 0053f030 / 0053f130 (ECX = index, arg = loader; finish a vehicle / a rider
 * whose packages arrived) are not in the core yet. */
static uint32_t part_packages_ready(rx_addr part, uint32_t arg) {
    uint32_t r;
    RACE_CORE_CALL(1, r = rx_rider_part_packages_ready(part, arg));
    return r;
}
static void package_cache_update(rx_addr cache, float dt) {
    RACE_CORE_CALL(1, rx_pkgcache_update(cache, dt));
}
static void loader_finish(rx_addr fn, uint32_t index, rx_addr loader) {
    rx_regs in; memset(&in, 0, sizeof in); in.r[1] = index;
    rx_call_original_ex(fn, &in, 0, 1, &loader);
}

/* 0053cb80 (arg = vehicle parts; RET 4) -> EAX: has any of the 8 part slots (+0x3c) its packages (0053ca70 with +0x14)? */
int rx_loader_parts_ready(rx_addr parts) {
    for (uint32_t i = 0; i < 8; i++) {
        rx_addr part = U32(parts, 0x3c + 4 * i);
        if (part != 0 && part_packages_ready(part, U32(parts, 0x14)) != 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x0053cb80, rx_loader_parts_ready)

/* The parts record whose id (+4) matches. */
static rx_addr find_parts(rx_addr loader, uint32_t id) {
    for (int32_t j = 0; j < I32(loader, 0x18); j++) {
        rx_addr p = U32(U32(loader, 0x14), 4 * (uint32_t)j);
        if (U32(p, 4) == id) return p;
    }
    return 0;
}

/* 0053cc90 (ESI = loader, EAX = cache owner, arg = dt; RET 4): advance the package cache (+8), then finish each pending vehicle
 * (+0x28, count +0x2c; parts id at +0x5b51c) and rider (+0x3c, count +0x40; id at +0x350) whose parts are ready. */
void rx_loader_update(rx_addr loader, rx_addr owner, float dt) {
    package_cache_update(owner + 8, dt);
    for (int32_t i = 0; i < I32(loader, 0x2c); i++) {
        rx_addr parts = find_parts(loader, U32(U32(U32(loader, 0x28), 4 * (uint32_t)i), 0x5b51c));
        /* BOUNDARY (not reached): the finish of a pending vehicle / rider loader entry whose part packages arrived (0053f030 / 0053f130). The product's only pending entry has no parts record (find_parts answers 0, observed at race frame 1200), so rx_loader_parts_ready never holds, and the original's reference world executes 0 of these bytes in 1700 frames. */
        if (rx_loader_parts_ready(parts)) loader_finish(0x0053f030, (uint32_t)i, loader);
    }
    for (int32_t i = 0; i < I32(loader, 0x40); i++) {
        rx_addr parts = find_parts(loader, U32(U32(U32(loader, 0x3c), 4 * (uint32_t)i), 0x350));
        /* BOUNDARY (not reached): the finish of a pending vehicle / rider loader entry whose part packages arrived (0053f030 / 0053f130). The product's only pending entry has no parts record (find_parts answers 0, observed at race frame 1200), so rx_loader_parts_ready never holds, and the original's reference world executes 0 of these bytes in 1700 frames. */
        if (rx_loader_parts_ready(parts)) loader_finish(0x0053f130, (uint32_t)i, loader);
    }
}
RX_FUNCTION(0x0053cc90, rx_loader_update)

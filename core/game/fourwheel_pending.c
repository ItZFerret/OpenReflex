/* Three original271-byte vf8 bodies share the same instruction layout.
 * This source preserves the entire pending-reload path, not only its usual
 * no-pending branch. Missing guest loader/destructor remain explicit boundaries.
 * No RX_FUNCTION registration or product activation in this candidate. */
#include "fourwheel_pending.h"
#include "fx_util.h"
#include "build_vehicle_util.h"
#include "build_vehicle.h"
#include "build_xml.h"
#include "fourwheel_params.h"
#include "fourwheel_xml_packet.h"
#include "tuning_registry.h"
#ifndef RX_ORACLE
#include "fourwheel_guest_reader_binding.h"
#include "guest_reader_destroy.h"
#endif
#include "../src/rx_os.h"
#include "loop_jobs.h"

typedef struct {
    rx_addr code, retry, apply_address, mass_address;
    uint32_t (*apply)(rx_addr, rx_addr);
    void (*mass)(rx_addr);
} pending_kind;

/* Original/oracle callees reserve their own call frame. Repair only the return
 * slot afterwards: consumed argument writes belong to the actual callee. */
static uint32_t dependency(rx_addr caller, rx_addr ra, rx_addr target,
                           enum rx_conv conv, rx_addr self, unsigned n,
                           const uint32_t* args) {
#ifndef RX_ORACLE
    if (target == 0x8252c0) {
        if (conv != RX_STDCALL || n != 2) rx_fourwheel_reader_unresolved(target);
        rx_addr e=caller-12;
        SU32(e,0,ra);SU32(e,4,args[0]);SU32(e,8,args[1]);rx_set_sp(e);
        uint32_t value=rx_fourwheel_guest_reader_load(args[0],args[1]);
        if(rx_sp()!=e)rx_fourwheel_reader_unresolved(target);
        rx_set_sp(caller);return value;
    }
    if (target == 0x824fe0) {
        if (n != 0) rx_fourwheel_reader_unresolved(target);
        rx_addr e=caller-4;SU32(e,0,ra);rx_set_sp(e);
        /* Original ESI-only body: explicit typed self, not stack-ABI dispatch. */
        rx_guest_reader_destroy_exact(fx_reg(FX_ESI));
        if(rx_sp()!=e)rx_fourwheel_reader_unresolved(target);
        uint32_t value=fx_reg(FX_EAX);rx_set_sp(caller);return value;
    }
    /* No generic guessed LTCG ABI for an unknown dependency. Native apply,
     * mass, registry/publication and cookie paths already use their typed owners. */
    rx_fourwheel_reader_unresolved(target);
#endif
    rx_set_sp(caller);
    uint32_t value = rx_call_original(target, conv, self, (int)n, args);
    SU32(caller - 4u * (n + 1u), 0, ra);
    rx_set_sp(caller);
    return value;
}

static void frame(rx_addr caller, rx_addr ra, unsigned n,
                  const uint32_t* args) {
    rx_addr entry = caller - 4u * (n + 1u);
    SU32(entry, 0, ra);
    for (unsigned i = 0; i < n; ++i) SU32(entry, 4u + 4u * i, args[i]);
    rx_set_sp(entry);
}

static void system_call(rx_addr caller, rx_addr ra, rx_addr slot) {
    const uint32_t cs = 0xad7538;
    frame(caller, ra, 1, &cs);
    rx_set_sp(caller);
    rx_os_import(slot, 1, &cs);
    SU32(caller - 8, 0, ra);
    rx_set_sp(caller);
}

static void pending(rx_addr sim, const pending_kind* kind) {
    const rx_addr entry = rx_sp(), bp = entry - 4;
    const rx_addr reader = (bp & ~7u) - 0x468, caller = reader - 8;
    const fx_saved saved = fx_save();
    SU32(bp, 0, saved.ebp);
    fx_set_reg(FX_EBP, bp);
    uint32_t cookie = U32(0, 0x9b45e4) ^ reader;
    fx_set_reg(FX_EAX, cookie);
    SU32(reader, 0x464, cookie);
    SU32(reader - 4, 0, saved.esi);
    SU32(reader - 8, 0, saved.edi);
    fx_set_reg(FX_EDI, sim);
    rx_set_sp(caller);

    if (U32(0, 0x9e3100) && U32(0, kind->retry)) {
        system_call(caller, kind->code + 0x43, 0x91600c);
        const uint32_t zero = 0;
        fx_set_reg(FX_ESI, reader);
#ifdef RX_ORACLE
        dependency(caller, kind->code + 0x4e, 0x824e70,
                   RX_CDECL, 0, 1, &zero);
#else
        frame(caller, kind->code + 0x4e, 1, &zero);
        rx_xml_reader_construct(reader);
        rx_set_sp(caller);
#endif
        const uint32_t load_args[] = {reader, U32(0, 0x9e3100)};
        fx_set_reg(FX_EAX, load_args[1]);
        fx_set_reg(FX_ECX, reader);
        /* Guest polymorphic stream ABI. The existing host rx_stream loader is
         * not a substitute; this remains fatal in native until fully closed. */
        uint32_t loaded = dependency(caller, kind->code + 0x5c,
                                    0x8252c0, RX_STDCALL, 0, 2, load_args);
        if (loaded) {
            const uint32_t apply_args[] = {sim + 0x2f10, reader};
            fx_set_reg(FX_EDX, reader);
            fx_set_reg(FX_EAX, apply_args[0]);
            uint32_t ok;
#ifdef RX_ORACLE
            ok = dependency(caller, kind->code + 0x6f, kind->apply_address,
                            RX_STDCALL, 0, 2, apply_args);
#else
            frame(caller, kind->code + 0x6f, 2, apply_args);
            ok = kind->apply(apply_args[0], reader);
            rx_set_sp(caller);
#endif
            fx_set_reg(FX_ESI, ok);
            if (ok) {
                /* Three publication arguments remain pushed across the getter;
                 * the fourth pushECX is only a discarded stack reservation. */
                SU32(caller - 4, 0, 0);
                SU32(caller - 8, 0, 0);
                SU32(caller - 12, 0, 10);
                SU32(caller - 16, 0, fx_reg(FX_ECX));
                rx_addr sink;
#ifdef RX_ORACLE
                sink = dependency(caller - 16, kind->code + 0x81,
                                  0x42fed0, RX_CDECL, 0, 0, 0);
#else
                frame(caller - 16, kind->code + 0x81, 0, 0);
                sink = rx_tuning_registry_get();
#endif
                fx_set_reg(FX_EAX, sink);
                const uint32_t publish_args[] = {sink, 10, 0, 0};
#ifdef RX_ORACLE
                dependency(caller, kind->code + 0x8a, 0x790540,
                           RX_STDCALL, 0, 4, publish_args);
                fx_set_reg(FX_EDX, sim);
                dependency(caller, kind->code + 0x91, kind->mass_address,
                           RX_CDECL, 0, 0, 0);
#else
                frame(caller, kind->code + 0x8a, 4, publish_args);
                rx_tuning_packet_publish(sink, 10, 0, 0);
                fx_set_reg(FX_EDX, sim);
                frame(caller, kind->code + 0x91, 0, 0);
                kind->mass(sim);
                rx_set_sp(caller);
#endif
            }
            /* Re-read only after mass setup; original deliberately does not
             * retain the source pointer used for the earlier load. */
            rx_addr stream = U32(0, 0x9e3100);
            fx_set_reg(FX_ECX, stream);
            if (!ok) {
                SU32(0, kind->retry, 0);
                SU32(stream, 0xc, 0);
            } else {
                if (stream) {
                    rx_addr vt = U32(stream, 0), fn = U32(vt, 0);
                    fx_set_reg(FX_EDX, vt);
                    fx_set_reg(FX_EAX, fn);
                    const uint32_t one = 1;
#ifdef RX_ORACLE
                    dependency(caller, kind->code + 0xc0, fn,
                               RX_THISCALL, stream, 1, &one);
#else
                    void* destroy = rx_fn(fn);
                    if (fn != 0x810890 || U32(stream,0) != 0x974d1c)
                        rx_fourwheel_reader_unresolved(fn);
                    if (destroy) {
                        frame(caller, kind->code + 0xc0, 1, &one);
                        ((rx_addr (*)(rx_addr, uint32_t))destroy)(stream, one);
                        rx_set_sp(caller);
                    } else {
                        dependency(caller, kind->code + 0xc0, fn,
                                   RX_THISCALL, stream, 1, &one);
                    }
#endif
                }
                SU32(0, 0x9e3100, 0);
                SU32(0, kind->retry, 1);
            }
        }
        system_call(caller, kind->code + 0xdf, 0x9160f8);
        fx_set_reg(FX_ESI, reader);
        /* ESI-only original destructor; semantic host helper omits embedded
         * stream teardown. Exact dependency is being ported separately. */
        dependency(caller, kind->code + 0xe8, 0x824fe0, RX_CDECL, 0, 0, 0);
    }
    if (!U32(0, 0x9e3100)) SU32J(0, kind->retry, 1);   /* (every simulation of the class stores it: SU32J, loop_jobs.h) */
    fx_set_reg(FX_ECX, U32(reader, 0x464) ^ reader);
    fx_set_reg(FX_EDI, saved.edi);
    fx_set_reg(FX_ESI, saved.esi);
#ifdef RX_ORACLE
    dependency(reader, kind->code + 0x10b, 0x8f9efe, RX_CDECL, 0, 0, 0);
#else
    frame(reader, kind->code + 0x10b, 0, 0);
    phys_security_check_cookie(fx_reg(FX_ECX));
#endif
    fx_set_reg(FX_EBP, saved.ebp);
    rx_set_sp(entry);
}

static const pending_kind utv = {0x4cbf20, 0x9cc8bc, 0x4cb3f0, 0x4cc030,
    rx_utv_params_apply, rx_sim_utv_setup_masses};
static const pending_kind buggy = {0x4b16f0, 0x9cc8b8, 0x4b0bc0, 0x4b1800,
    rx_buggy_params_apply, rx_sim_buggy_setup_masses};
static const pending_kind truck = {0x450370, 0x9cc89c, 0x44f7a0, 0x450480,
    rx_truck2wd_params_apply, rx_sim_truck_setup_masses};
void rx_utv_pending_tuning(rx_addr sim) { pending(sim, &utv); }
void rx_buggy_pending_tuning(rx_addr sim) { pending(sim, &buggy); }
void rx_truck_pending_tuning(rx_addr sim) { pending(sim, &truck); }

/* Isolated RaceFinishScenario state selection. Integer-only original helpers;
 * no startup, script execution, podium/replay substitute or Post guard change.
 * fx_reg is the existing CPU observer (FxBind in oracle, register model in product).
 * Structural register writes and saved stack words retain the recorded live EDX
 * and genuine nested-callee x87 effects rather than inventing output values. */
#include "race_finish_select.h"
#include "fx.h"
#include "fx_util.h"
#include "build_vehicle_util.h"
#include "rider_cap.h"

static uint32_t finish_return(rx_addr entry, fx_saved saved, uint32_t result) {
    fx_restore(saved);
    fx_set_reg(FX_EAX, result);
    rx_set_sp(entry);
    return result;
}

/* The original getter is a dependency, not a replay of any candidate entry.
 * Oracle executes its real initialization/CRT boundary at caller ESP; product
 * calls the existing registered series-manager constructor/getter implementation.
 * External call helpers push their own return; patch only that sentinel slot. */
static rx_addr series_manager(rx_addr caller, uint32_t return_address) {
    rx_addr result;
#ifdef RX_ORACLE
    rx_set_sp(caller);
    result = rx_call_original(0x00413d20, RX_CDECL, 0, 0, 0);
    SU32(caller - 4, 0, return_address);
#else
    veh_call_sp(caller, return_address, 0, 0, 0, 0);
    result = rx_fx_series_manager();
#endif
    fx_set_reg(FX_EAX, result);
    rx_set_sp(caller);
    return result;
}

/*0074a070: switch table at0074a0ec maps3/4/6/7 to the podium branch,
 *5 to winner,0/1/2 and unsigned values>7 to the zero-stage branch. */
uint32_t rx_finish_select_advance(rx_addr sc, uint32_t kind) {
    fx_set_reg(FX_EAX, sc);
    fx_set_reg(FX_ECX, kind);
    if (kind == 3 || kind == 4 || kind == 6 || kind == 7) {
        uint32_t delta = U32(sc, 0x151b4) - 7u;
        fx_set_reg(FX_ECX, delta);
        if (delta == 0) {
            uint32_t stage = U32(sc, 0x151c0) == 0 ? 12u : 10u;
            SU32(sc, 0x151b8, 1);
            fx_set_reg(FX_ECX, stage);
            SU32(sc, 0x151b4, stage);
            fx_set_reg(FX_EAX, 1);
            return 1;
        }
        delta -= 3u;
        fx_set_reg(FX_ECX, delta);
        if (delta != 0) fx_set_reg(FX_ECX, delta - 2u);
    } else if (kind == 5) {
        uint32_t delta = U32(sc, 0x151b4) - 7u;
        fx_set_reg(FX_ECX, delta);
        if (delta == 0 && U32(sc, 0x151c4) != 0) {
            SU32(sc, 0x151b4, 8);
            SU32(sc, 0x151b8, 1);
            fx_set_reg(FX_EAX, 1);
            return 1;
        }
    }
    SU32(sc, 0x151b4, 0);
    fx_set_reg(FX_EAX, 0);
    return 0;
}
RX_FUNCTION(0x0074a070, rx_finish_select_advance)

/*0074a110: ECX=sc, push ESI/EDI; EBX is pushed only after the fixed-stage
 * override. Stage/debug comparisons are signed, unlike advance's kind switch. */
uint32_t rx_finish_select_next(rx_addr sc) {
    const rx_addr entry = rx_sp();
    const fx_saved saved = fx_save();
    SU32(entry - 4, 0, saved.esi);
    fx_set_reg(FX_ESI, sc);
    const uint32_t stage = U32(sc, 0x151b4);
    fx_set_reg(FX_EAX, stage);
    SU32(entry - 8, 0, saved.edi);
    fx_set_reg(FX_EDI, 1);
    if (stage == U32(0, 0xd610a0)) return finish_return(entry, saved, 1);

    SU32(entry - 12, 0, saved.ebx);
    fx_set_reg(FX_EBX, 0);
    const rx_addr caller = entry - 12;
    rx_set_sp(caller);
    if (U32(0, 0xd61048) != 0) {
        const uint32_t result = (int32_t)stage < 12 ? 1u : 0u;
        SU32(sc, 0x151b4, result ? stage + 1u : 0u);
        return finish_return(entry, saved, result);
    }

    const rx_addr act = U32(sc, 0x14);
    fx_set_reg(FX_EAX, act);
    if (U32(act, 0x238) == 8) {
        SU32(sc, 0x151b4, 0);
        return finish_return(entry, saved, 0);
    }
    rx_addr manager = series_manager(caller, 0x0074a166);
    uint32_t kind = U32(manager, 0x190);
    uint32_t series = kind == 2;
    if (!series) {
        manager = series_manager(caller, 0x0074a174);
        fx_set_reg(FX_ECX, 1);
        kind = U32(manager, 0x190);
        series = kind == 1;
    }
    if (series) {
        manager = series_manager(caller, 0x0074a1be);
        const rx_addr event = U32(manager, 0x18c);
        fx_set_reg(FX_EAX, event);
        uint32_t event_kind = U32(event, 0xc);
        fx_set_reg(FX_ECX, event_kind);
        if (event_kind == 2) {
            event_kind = U32(event, 0x10);
            fx_set_reg(FX_ECX, event_kind);
        }
        fx_set_reg(FX_EAX, sc);
        veh_call_sp(caller, 0x0074a1d6, 0, 0, 0, 0);
        const uint32_t result = rx_finish_select_advance(sc, event_kind);
        return finish_return(entry, saved, result);
    }
    /* Original reloads the stage after the getter calls. */
    uint32_t delta = U32(sc, 0x151b4) - 7u;
    fx_set_reg(FX_EAX, delta);
    /* ECX is 1 here (0074a174: mov ecx, edi = 1). The value is written as the constant: the register model (fx_reg) only holds the
     * CPU's registers under an FxBind adapter, and this function is also reached natively from the scenario's start / update
     * (rx_scenario_start, rx_race_finish_vf0's tail call), where the model would read 0 in the oracle build (w-podium). */
    if (delta == 0) {
        if (U32(sc, 0x151c4) != 0) {
            SU32(sc, 0x151b4, 8);
            SU32(sc, 0x151b8, 1);
            return finish_return(entry, saved, 1);
        }
    } else {
        delta -= 1u;
        fx_set_reg(FX_EAX, delta);
        if (delta != 0) return finish_return(entry, saved, 1);
    }
    SU32(sc, 0x151b4, 0);
    return finish_return(entry, saved, 0);
}
RX_FUNCTION(0x0074a110, rx_finish_select_next)

/*0074f0f0: genuine vf8 call. BaseScenario::vf8 is registered0055eb10;
 * unknown dynamic targets keep the normal fatal product boundary. */
uint32_t rx_finish_select_previous(rx_addr sc) {
    const rx_addr entry = rx_sp(), caller = entry - 8;
    const fx_saved saved = fx_save();
    SU32(entry - 4, 0, saved.esi);
    fx_set_reg(FX_ESI, sc);
    const rx_addr vt = U32(sc, 0), fn = U32(vt, 0x20);
    fx_set_reg(FX_EAX, vt);
    fx_set_reg(FX_EDX, fn);
    SU32(entry - 8, 0, saved.edi);
    fx_set_reg(FX_EDI, 0);
    uint32_t done;
#ifdef RX_ORACLE
    rx_set_sp(caller);
    done = rx_call_original(fn, RX_THISCALL, sc, 0, 0);
    SU32(caller - 4, 0, 0x0074f0fd);
#else
    void* target = rx_fn(fn);
    if (target) {
        veh_call_sp(caller, 0x0074f0fd, 0, 0, 0, 0);
        done = ((uint32_t (*)(rx_addr))target)(sc);
    } else {
        rx_set_sp(caller);
        done = rx_call_original(fn, RX_THISCALL, sc, 0, 0);
    }
#endif
    rx_set_sp(caller);
    uint32_t result = 0;
    if (done == 0) {
        const uint32_t stage = U32(sc, 0x151b4);
        fx_set_reg(FX_EAX, stage);
        if ((int32_t)stage > 7) {
            fx_set_reg(FX_EAX, stage - 8u);
            if (stage - 8u <= 4u) {
                SU32(sc, 0x151b4, stage - 1u);
                result = 1;
            }
        }
    }
    return finish_return(entry, saved, result);
}
RX_FUNCTION(0x0074f0f0, rx_finish_select_previous)

/*0074f1a0: EAX=sc stays unchanged. EDX is the final standings player in
 * the normal positive-count path; replay/count<=0 leave incoming EDX alone. */
void rx_finish_select_local_flags(rx_addr sc) {
    const rx_addr entry = rx_sp();
    const fx_saved saved = fx_save();
    SU32(entry - 4, 0, saved.ebx);
    SU32(entry - 8, 0, saved.ebp);
    fx_set_reg(FX_EBX, 0);
    SU32(entry - 12, 0, saved.esi);
    const rx_addr act = U32(sc, 0x14);
    fx_set_reg(FX_ESI, act);
    SU32(sc, 0x151c4, 0);
    SU32(sc, 0x151c0, 0);
    SU32(sc, 0x151c8, 0);
    const int32_t count = I32(act, 0x26c);
    fx_set_reg(FX_EBP, (uint32_t)count);
    SU32(entry - 16, 0, saved.edi);
    if (U32(act, 0x238) == 8) {
        const rx_addr first = U32(act, 0x2d0);
        fx_set_reg(FX_ECX, first);
        const uint32_t winner = U32(first, 0x1128) == 0;
        SU32(sc, 0x151c4, winner);
        SU32(sc, 0x151c0, winner);
        SU32(sc, 0x151c8, !winner);
    } else {
        fx_set_reg(FX_ECX, 0);
        if (count > 0) fx_set_reg(FX_EDI, 1);
        for (int32_t i = 0; i < count; i++) {
            const rx_addr driver = rx_stand(act, (uint32_t)i);
            fx_set_reg(FX_EDX, driver);
            const rx_addr player = U32(driver, 0x74);
            fx_set_reg(FX_EDX, player);
            if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
                if (i <= 2) {
                    if (i == 0) SU32(sc, 0x151c4, 1);
                    SU32(sc, 0x151c0, 1);
                } else SU32(sc, 0x151c8, 1);
            }
            fx_set_reg(FX_ECX, (uint32_t)i + 1u);
        }
    }
    (void)finish_return(entry, saved, sc);
}
RX_FUNCTION(0x0074f1a0, rx_finish_select_local_flags)

uint32_t rx_finish_select_cancel(rx_addr sc) {
    SU32(sc, 0x151b4, 0);
    fx_set_reg(FX_EAX, 0);
    return 0;
}
RX_FUNCTION(0x0074fad0, rx_finish_select_cancel)

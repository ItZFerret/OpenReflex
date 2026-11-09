#!/bin/bash
# stop_audit_trace.sh <core build dir (from core/build.sh)> <out dir> -- builds the development librxcore the audit drivers use:
# the product objects with rx_mem.c replaced by a copy whose rx_call_original(_ex) log "RXTRACE-UNPORTED" and return 0 (instead of
# aborting), plus stop_audit_trace_hooks.c. Then: clang -I core/include core/tools/stop_audit_run.c -L<out> -lrxcore -Wl,-rpath,<out>.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); B=${1:?core build dir}; O=${2:?out dir}; mkdir -p "$O"
python3 - "$C/src/rx_mem.c" "$O/rx_mem_trace.c" <<'PY'
import sys
t = open(sys.argv[1]).read()
t = t.replace('#include <stdlib.h>\n#ifdef _WIN32', '#include <stdlib.h>\n#include <dlfcn.h>\n#include <string.h>\n#ifdef _WIN32', 1)
log = r'''
static void rxtrace_hit(uint32_t fn, int ex, void* ra) {
    static uint32_t seen_fn[4096], seen_ra[4096]; static int n = 0;
    Dl_info di; uintptr_t off = 0;
    for (int i = 0; i < n; i++) if (seen_fn[i] == fn) { /* cheap dedupe by target first */ if (seen_ra[i] == (uint32_t)(uintptr_t)ra) return; }
    if (dladdr(ra, &di)) off = (uintptr_t)ra - (uintptr_t)di.dli_fbase;
    if (n < 4096) { seen_fn[n] = fn; seen_ra[n] = (uint32_t)(uintptr_t)ra; n++; }
    fprintf(stderr, "RXTRACE-UNPORTED fn=%08x ex=%d caller_off=0x%lx\n", fn, ex, (unsigned long)off);
}
'''
t = t.replace('uint32_t rx_call_original(rx_addr fn, enum rx_conv conv', log + 'uint32_t rx_call_original(rx_addr fn, enum rx_conv conv', 1)
t = t.replace('''    fprintf(stderr, "rxcore: unported game function %08x reached\\n", fn);
    rx_mem_backtrace();
    abort();
}
void rx_call_original_ex''', '''    rxtrace_hit(fn, 0, __builtin_return_address(0));
    return 0;
}
void rx_call_original_ex''', 1)
t = t.replace('''    (void)in; (void)out; (void)nargs; (void)args;
    fprintf(stderr, "rxcore: unported game function %08x reached\\n", fn);
    rx_mem_backtrace();
    abort();''', '''    (void)in; (void)nargs; (void)args;
    rxtrace_hit(fn, 1, __builtin_return_address(0));
    if (out) memset(out, 0, sizeof *out);''', 1)
open(sys.argv[2], 'w').write(t)
PY
FLAGS=(-std=c11 -O2 -mcpu=apple-m1 -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fvisibility=hidden -fPIC -I"$C/include" -I"$C/src" -I"$C/game")
clang "${FLAGS[@]}" -c -o "$O/rx_mem_trace.o" "$O/rx_mem_trace.c"
clang "${FLAGS[@]}" -c -o "$O/hooks.o" "$C/tools/stop_audit_trace_hooks.c"
( cd "$B/obj" && ls *.o | grep -v '^src_rx_mem_c.o$' | xargs clang++ -arch arm64 -dynamiclib -install_name "$O/librxcore.dylib" -o "$O/librxcore.dylib" "$O/rx_mem_trace.o" "$O/hooks.o" -lz )
echo "built $O/librxcore.dylib"

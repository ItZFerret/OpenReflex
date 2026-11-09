#!/bin/bash
# vtrace_build.sh <out dir> -- a TRACED copy of the core for tools/vtrace_check.py (w-vtlog): core/build.sh with -DRX_FN_TRACE -g
# -fno-omit-frame-pointer into <out dir> (never the product's core/build: the normal build has no switch and is byte-for-byte what it was),
# dsymutil for the return-address symbolication, and the headless drivers that go through the session API linked against it:
#   <out>/stop_audit_run, stop_audit_twice, test_drive_test, career_series_test       (dynamic, against <out>/librxcore.dylib)
#   <out>/challenge_native_test.static, activation_native_test.static, champ_native_test.static   (static, product_link.sh's rx_link_driver)
# Run: RX_FN_TRACE_OUT=<file> <out>/stop_audit_run ... ; the file is written at exit, on SIGUSR1 and on a fatal signal.
# Then: _re/.venv/bin/python core/tools/sigcheck.py --vt-dump=<static.json>; python3 core/tools/vtrace_check.py <static.json> <traces> --lib <out>/librxcore.dylib
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=$1
[ -n "$O" ] || { echo "usage: vtrace_build.sh <out dir (absolute)>"; exit 2; }
case "$O" in /*) ;; *) echo "the out dir must be absolute"; exit 2;; esac
mkdir -p "$O"
RX_BUILD_JOBS=${RX_BUILD_JOBS:-8} RX_BUILD_EXTRA_CFLAGS="-DRX_FN_TRACE -g -fno-omit-frame-pointer" bash "$C/build.sh" "$O" > "$O/build.log" 2>&1 || { tail -20 "$O/build.log"; exit 1; }
dsymutil "$O/librxcore.dylib" 2> /dev/null
for t in stop_audit_run stop_audit_twice test_drive_test career_series_test; do
  clang -std=c11 -O1 -I "$C/include" -I "$C/src" -I "$C/game" -I "$C/api" "$C/tools/$t.c" -o "$O/$t" -L"$O" -lrxcore -Wl,-rpath,"$O" || echo "driver $t: build failed"
done
source "$C/tools/product_link.sh"
RX_PRODUCT_OBJS=(); while IFS= read -r l; do RX_PRODUCT_OBJS+=("$l"); done < "$O/objs.list"
for t in challenge_native_test activation_native_test champ_native_test; do
  rx_link_driver "$O/$t.static" "$C/tools/$t.c" || echo "driver $t: link failed"
done
echo "traced build in $O"

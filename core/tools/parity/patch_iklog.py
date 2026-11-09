# patch_iklog.py <core/game dir> <out src dir> -- w-parity2 (development only): a copy of anim_ik.c whose two-bone limb solve (006b0eb0)
# logs its entry and exit through core/tools/parity/ik_log.inc (PROBE_PARITY_LOG=<file>, PARITY_IKTGT, PARITY_IKPOSE).
import sys, os
g, o = sys.argv[1], sys.argv[2]
f = 'anim_ik.c'; s = open(os.path.join(g, f)).read()
old = "void rx_ik_two_bone_solve(rx_addr skel, rx_addr limb, rx_addr pose, float dt) {\n"
assert s.count(old) == 1
hook = ("#include <stdio.h>\n#define PAR_RD(a) U32(0, (rx_addr)(a))\n#include \"%s\"\n#undef PAR_RD\n"
        "static FILE* ik_lf(void) { static FILE* f; if (!f) { const char* p = getenv(\"PROBE_PARITY_LOG\"); f = p && *p ? fopen(p, \"w\") : NULL; } return f; }\n"
        "static void two_bone_impl(rx_addr skel, rx_addr limb, rx_addr pose, float dt);\n"
        "void rx_ik_two_bone_solve(rx_addr skel, rx_addr limb, rx_addr pose, float dt) {\n"
        "    const unsigned long s = ik_entry(ik_lf(), limb, pose);\n    two_bone_impl(skel, limb, pose, dt);\n    ik_exit(ik_lf(), s, limb, pose);\n}\n"
        % os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ik_log.inc'))
s = s.replace(old, hook + "static void two_bone_impl(rx_addr skel, rx_addr limb, rx_addr pose, float dt) {\n")
open(os.path.join(o, f), 'w').write('#line 1 "%s"\n' % f + s)

# patch_randlog.py <core/game dir> <out src dir> -- w-parity2 (development only): a copy of race_driver_update.c whose AI track-node
# reactions (005118c0) log the game LCG at their entry through core/tools/parity/rand_log.inc (PROBE_PARITY_LOG=<file>).
import sys, os
g, o = sys.argv[1], sys.argv[2]
f = 'race_driver_update.c'; s = open(os.path.join(g, f)).read()
old = "void rx_ai_track_node_reactions(rx_addr d, float dt) {\n"
assert s.count(old) == 1
hook = ("#include <stdio.h>\n#include <stdlib.h>\n#define PAR_RD(a) U32(0, (rx_addr)(a))\n#include \"%s\"\n#undef PAR_RD\n"
        "static FILE* rn_lf(void) { static FILE* f; if (!f) { const char* p = getenv(\"PROBE_PARITY_LOG\"); f = p && *p ? fopen(p, \"w\") : NULL; } return f; }\n"
        % os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rand_log.inc'))
s = s.replace(old, hook + old + "    rn_entry(rn_lf(), d);\n")
open(os.path.join(o, f), 'w').write('#line 1 "%s"\n' % f + s)

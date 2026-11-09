# patch_cyllog.py <core/game dir> <out src dir> -- w-parity2 (development only): a copy of phys_pair3.c whose CylinderVsCylinder
# (0086bd80) logs its entry stack through core/tools/parity/cyl_log.inc (PROBE_PARITY_LOG=<file>, PARITY_NEAR=x,z,r).
import sys, os
g, o = sys.argv[1], sys.argv[2]
d = os.path.dirname(os.path.abspath(__file__))
f = 'phys_pair3.c'; s = open(os.path.join(g, f)).read()
old = "uint32_t phys_pair_cylinder_cylinder(rx_addr qb, rx_addr pb, rx_addr M, rx_addr A, rx_addr qa, rx_addr pa, rx_addr B, uint32_t flag,"
assert s.count(old) == 1
i = s.index(old); j = s.index("{\n", i) + 2
hook = ("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#define PAR_RD(a) U32(0, (rx_addr)(a))\n#define CY_WRITE(a, w) SU32(0, (rx_addr)(a), (w))\n#include \"%s\"\n#include \"%s\"\n#undef PAR_RD\n"
        "static FILE* cy_lf(void) { static FILE* f; if (!f) { const char* p = getenv(\"PROBE_PARITY_LOG\"); f = p && *p ? fopen(p, \"w\") : NULL; } return f; }\n"
        % (os.path.join(d, '..', 'parity_pair.inc'), os.path.join(d, 'cyl_log.inc')))
s = s[:i] + hook + s[i:j] + "    cy_entry(cy_lf(), rx_sp(), pa);\n" + s[j:]
open(os.path.join(o, f), 'w').write('#line 1 "%s"\n' % f + s)

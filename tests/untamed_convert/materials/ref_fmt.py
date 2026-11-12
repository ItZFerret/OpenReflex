#!/usr/bin/env python3
"""Reference for the number formatting the materials stage writes into its reason strings ('%g', '%+g', '%.3f', '%.0f', repr, round):
<out dir>/fmt.json = [[float.hex(x), '%g' % x, '%+g' % x, '%.3f' % x, '%.0f' % x, repr(x), float.hex(round(x, 6)), float.hex(round(x, 3))], ...]
for fixed edge cases and 20,000 seeded random doubles. Standard library only; run with -I -B.

    python.exe -I -B ref_fmt.py <out dir>
"""
import json, random, struct, sys
from pathlib import Path

out = Path(sys.argv[1])
rnd = random.Random(20261111)
xs = [0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.5, 3.5, 0.0005, 0.0015, 0.0025, 1e-4, 9.9999949e-5, 9.9999951e-5, 0.000123456789, 123456.5, 999999.5, 999999.4,
      1e6, 1234567.0, 1e16, 1e-5, 0.01, 0.25, 0.1, 0.2, 0.3, 1.16, 2.08, 3.0, 0.157, 0.18, 0.502, 100.0, 255.0, 0.396078, 1e22, 5e-324, 1.7976931348623157e308,
      0.1 + 0.2, 2.675, 1.005, 0.125, 0.375, 1e-7, 123456789.0, 0.99999949, 0.9999995, 99999.95, 0.00099995]
for _ in range(20000):
    k = rnd.random()
    if k < 0.3:
        xs.append(rnd.uniform(-2, 2))
    elif k < 0.5:
        xs.append(round(rnd.uniform(-300, 300), rnd.randint(0, 6)))
    elif k < 0.7:
        xs.append(struct.unpack('<f', struct.pack('<f', rnd.uniform(-4, 4)))[0])
    elif k < 0.85:
        xs.append(rnd.uniform(-1, 1) * 10.0 ** rnd.randint(-12, 12))
    else:
        xs.append(rnd.randint(-2000, 2000) / 2.0 ** rnd.randint(0, 12))
rows = [[x.hex(), '%g' % x, '%+g' % x, '%.3f' % x, '%.0f' % x, repr(x), round(x, 6).hex(), round(x, 3).hex()] for x in xs]
out.mkdir(parents=True, exist_ok=True)
(out / 'fmt.json').write_text(json.dumps(rows), encoding='utf-8')
print(len(rows), 'values')

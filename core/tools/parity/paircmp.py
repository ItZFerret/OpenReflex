import sys
# paircmp.py product.log oracle.log -- align the composite-pair logs by the call inputs (C lines minus seq / sh); report the first call
# whose result (c line) differs, and the first input mismatch
def load(p):
    calls = []; cur = None
    for l in open(p):
        t = l.split()
        if not t: continue
        if t[0] == 'C' and 'sh' in t:
            sh = t[t.index('sh') + 1]
            cur = [' '.join(t[2:t.index('sh')]), None, sh, int(t[1])]
        elif t[0] == 'c' and cur:
            cur[1] = ' '.join(t[2:]); calls.append(cur); cur = None
    return calls
a = load(sys.argv[1]); b = load(sys.argv[2])
# find the oracle's first call equal to the product's first
start = next((j for j in range(len(b)) if b[j][0] == a[0][0]), None)
print('product calls', len(a), 'oracle calls', len(b), 'oracle start', start)
i, j = 0, start
shd = 0; shown = 0
while i < len(a) and j < len(b):
    if a[i][0] != b[j][0]:
        print('INPUT differs at product call', a[i][3], 'oracle', b[j][3]); print(' P', a[i][0][:400]); print(' O', b[j][0][:400]); break
    if a[i][2] != b[j][2]: shd += 1
    if a[i][1] != b[j][1]:
        print('RESULT differs at product call', a[i][3], 'oracle', b[j][3], '(stack hash', 'same' if a[i][2] == b[j][2] else 'differs', ')')
        print(' in', a[i][0][:500]); print(' P', a[i][1][:700]); print(' O', b[j][1][:700]); shown += 1
        if shown >= 3: break
    i += 1; j += 1
print('compared', i, 'stack-hash differences', shd)

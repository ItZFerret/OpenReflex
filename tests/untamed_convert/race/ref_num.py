# tests/untamed_convert/race/ref_num.py <out file>
# Writes the reference values the race part's "numbers" test compares UxRaceNum against, bit for bit: the reference machine's numpy /
# OpenBLAS / CPython results for the operations the race kit's bytes depend on (np.linalg.inv / solve, the BLAS products, math.hypot,
# np.hypot, sum(), np.percentile / median, np.arange / linspace, np.random.default_rng, round(), '%f' / '%g').
# Doubles are written as 16 hex digits of their bits. Read-only; run with
#   $UX_PYTHON -I -B ref_num.py <scratch>/ref/num.json
import sys, json, math, struct
sys.dont_write_bytecode = True
import numpy as np


def hx(v):
    return struct.pack('>d', float(v)).hex()


def hxs(a):
    return [hx(v) for v in np.asarray(a, np.float64).reshape(-1)]


def main():
    rng = np.random.default_rng(20261111)
    o = {'numpy': np.__version__, 'python': sys.version.split()[0]}

    def zeros_signs(a, pz=0.3, pn=0.3):
        a = a.copy()
        a[rng.random(a.shape) < pz] = 0.0
        a[rng.random(a.shape) < pn] *= -1.0
        return a

    def frame():
        a = rng.uniform(0, 6.3); c, s = np.cos(a), np.sin(a)
        M = np.eye(4); M[0, 0] = c; M[0, 2] = -s; M[2, 0] = s; M[2, 2] = c; M[3, :3] = rng.uniform(-300, 300, 3)
        return M.astype(np.float32).astype(np.float64)

    inv = []
    for t in range(400):
        if t % 4 == 0:
            M = rng.standard_normal((4, 4))
        elif t % 4 == 1:
            M = frame()
        elif t % 4 == 2:
            q = np.linalg.qr(rng.standard_normal((3, 3)))[0]
            M = np.eye(4); M[:3, :3] = q * rng.uniform(0.5, 2); M[3, :3] = rng.uniform(-300, 300, 3)
        else:
            M = frame(); R = np.eye(4); R[0, 0] = -1; R[2, 2] = -1; M = R @ M
        inv.append([hxs(M), hxs(np.linalg.inv(M))])
    o['inv4'] = inv
    sol = []
    for t in range(400):
        A = rng.standard_normal((2, 2)) * 10 ** rng.uniform(-1, 2); b = rng.standard_normal(2) * 30
        if t % 5 == 0:
            A = zeros_signs(A, 0.2, 0.3) + np.eye(2) * 1e-3
        sol.append([hxs(A), hxs(b), hxs(np.linalg.solve(A, b))])
    o['solve2'] = sol
    mm = []
    for t in range(300):
        A = zeros_signs(rng.standard_normal((4, 4))); B = zeros_signs(rng.standard_normal((4, 4)))
        if t % 3 == 0:
            A = frame(); B = frame()
        if t % 7 == 0:
            A = np.eye(4); A[0, 0] = -1; A[2, 2] = -1
        mm.append([hxs(A), hxs(B), hxs(A @ B)])
    o['matmul4'] = mm
    tr = []
    for t in range(40):
        M = frame() if t % 2 else zeros_signs(rng.standard_normal((4, 4)))
        P = zeros_signs(rng.standard_normal((9, 3)) * 100, 0.1, 0.2)
        W = P @ M[:3, :3] + M[3, :3]
        T = P.reshape(3, 3, 3) @ M[:3, :3] + M[3, :3]
        assert W.tobytes() == T.tobytes()
        tr.append([hxs(M), hxs(P), hxs(W)])
    o['transform'] = tr
    vm = []
    for t in range(300):
        W = frame() if t % 2 else zeros_signs(rng.standard_normal((4, 4)))
        A = zeros_signs(rng.standard_normal((4, 4)), 0.2, 0.2)
        vm.append([hxs(W), hxs(A), hx((W[3] @ A)[0])])
    o['vecmat4'] = vm
    dots = []
    for t in range(600):
        n = 2 + t % 2
        a = zeros_signs(rng.standard_normal(n) * 50, 0.2, 0.2); b = zeros_signs(rng.standard_normal(n), 0.2, 0.2)
        dots.append([hxs(a), hxs(b), hx(a @ b), hx(np.linalg.norm(a))])
    o['dot'] = dots
    hyp = []
    for t in range(3000):
        x = float(rng.standard_normal() * 10 ** rng.uniform(-3, 3)); y = float(rng.standard_normal() * 10 ** rng.uniform(-3, 3))
        if t % 50 == 0: x = 0.0
        if t % 70 == 0: y = -0.0
        hyp.append([hx(x), hx(y), hx(math.hypot(x, y)), hx(np.hypot(x, y))])
    for x, y in ((1e200, 3e200), (1e-200, 2e-200), (3e-310, 4e-310), (5e-324, 0.0), (1e308, 1e308), (0.0, 0.0), (1.0, 1e-170)):
        hyp.append([hx(x), hx(y), hx(math.hypot(x, y)), hx(math.hypot(x, y))])
    o['hypot'] = hyp
    sums = []
    for t in range(200):
        n = int(rng.integers(1, 400))
        a = (rng.standard_normal(n) * 10 ** rng.uniform(-2, 3, n)).astype(np.float32).astype(np.float64).tolist()
        sums.append([hxs(a), hx(sum(a)), hx(float(np.median(a))), hx(float(np.percentile(a, 90))), hx(float(np.sum(np.array(a)))),
                     hx(float(np.mean(a[:16])))])
    o['sums'] = sums
    rnd = []
    for t in range(1500):
        x = float(rng.standard_normal() * 10 ** rng.uniform(-2, 3))
        if t % 10 == 0: x = round(x, 2) + 0.005
        if t % 10 == 1: x = round(x, 1) + 0.05
        rnd.append([hx(x), hx(round(x, 1)), hx(round(x, 2)), hx(round(x, 3)), hx(float(np.round(x, 2))), hx(float(np.round(x, 4))),
                    '%.0f' % x, '%.1f' % x, '%.2f' % x, '%g' % x])
    o['round'] = rnd
    r = np.random.default_rng(7)
    first = r.random(2000)
    p = rng.random(37); p /= p.sum()
    pick = r.choice(np.arange(37), size=300, p=p)
    u = r.random((300, 2))
    o['rng'] = dict(first=hxs(first[:16]), xor='%016x' % int(np.bitwise_xor.reduce(first.view(np.uint64))), p=hxs(p), pick=[int(v) for v in pick],
                    u=hxs(u[:8]), u_xor='%016x' % int(np.bitwise_xor.reduce(u.reshape(-1).view(np.uint64))))
    ar = []
    for z in (-0.572, 0.0, 1.337, -7.4, 0.428, -0.57, 12.25, -3.0, -1.0):
        if z in (-3.0, -1.0):
            a = np.arange(z, -z + 1e-9, 0.5)
            ar.append([hx(z), hx(-z + 1e-9), hx(0.5), hxs(a)])
        else:
            a = np.arange(z - 1.0, z + 30.0 + 1e-9, 0.5)
            ar.append([hx(z - 1.0), hx(z + 30.0 + 1e-9), hx(0.5), hxs(a)])
    o['arange'] = ar
    o['linspace'] = [[n, hxs(np.linspace(0, 1, n))] for n in (1, 2, 3, 4, 7, 50, 244, 257)]
    with open(sys.argv[1], 'w', encoding='utf-8') as f:
        json.dump(o, f)
    print('num.json: %d inv4, %d solve2, %d matmul4, %d hypot, %d sums, %d round' % (len(inv), len(sol), len(mm), len(hyp), len(sums), len(rnd)))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""fpv_revert_tweaks.py <fpvlog.csv[.gz]> ... -- w-fpvrevert: what HEAD SMOOTHING 0.035 -> 0.05 s and VIEW SMOOTHING 0 -> 0.03 s do to the restored
(7a9c99ff) Dynamic FPV camera, measured on a real ride's camera log of that camera (FpvRideProbe's fpvlog.csv, e.g. w-fpvrewrite's "before" rides
at 120 - 240 fps: REFLEX-lead/builds/fpvrewrite/logs/B-*.fpvlog.csv.gz). The two tweaks are linear filters, so they are applied to the logged view
exactly as the camera applies them:
  VIEW SMOOTHING  the view frame's up (q x AngleAxis(-lookDown, right), lookDown = PITCH x 20 + 15 (+ LEAN PITCH's lean x 10, the lean input filtered
                  at 3.5 / s as the camera does)) on a critically damped spring of time constant tau (the exact step form on the presentation clock),
                  the frame rebuilt with the same forward and the look down put back. (Defaults only: CORNER ANTICIPATION OFF, TILT 0, no free look.)
  HEAD SMOOTHING  the eye in the bike's frame (the old spring's output) through ((1 + s 0.035) / (1 + s tau))^2 = the spring of tau on the same target.
Printed per log: the view's chatter (its angular speed above ~3 Hz: the residual of a 50 ms low pass, RMS deg/s, total and per view axis) and the
eye's (ft/s) over 15 mph, before / after, and how far the tweaked view is from the logged one (angle / position, median, p95, max)."""
import csv, gzip, math, sys
import numpy as np

import os
PITCH, TAU_EYE_OLD = 0.31, 0.035
TAU_VIEW, TAU_EYE = float(os.environ.get("TAU_VIEW", "0.03")), float(os.environ.get("TAU_EYE", "0.05"))   # (the w-fpvrevert defaults; env to try others)

def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return np.array([aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz])
def qinv(q): return np.array([-q[0], -q[1], -q[2], q[3]])
def qrot(q, v):
    u = q[:3]; w = q[3]; t = 2*np.cross(u, v); return v + w*t + np.cross(u, t)
def axis_angle(axis, deg):
    h = math.radians(deg)/2; s = math.sin(h); return np.array([axis[0]*s, axis[1]*s, axis[2]*s, math.cos(h)])
def look_rotation(f, u):
    # Unity's Quaternion.LookRotation(forward, up)
    f = f/np.linalg.norm(f); r = np.cross(u, f); r /= np.linalg.norm(r); u2 = np.cross(f, r)
    m = np.array([r, u2, f]).T   # columns: x, y, z
    tr = m[0,0] + m[1,1] + m[2,2]
    if tr > 0:
        s = math.sqrt(tr + 1)*2; return np.array([(m[2,1]-m[1,2])/s, (m[0,2]-m[2,0])/s, (m[1,0]-m[0,1])/s, s/4])
    i = int(np.argmax([m[0,0], m[1,1], m[2,2]]))
    if i == 0: s = math.sqrt(1 + m[0,0] - m[1,1] - m[2,2])*2; return np.array([s/4, (m[0,1]+m[1,0])/s, (m[0,2]+m[2,0])/s, (m[2,1]-m[1,2])/s])
    if i == 1: s = math.sqrt(1 + m[1,1] - m[0,0] - m[2,2])*2; return np.array([(m[0,1]+m[1,0])/s, s/4, (m[1,2]+m[2,1])/s, (m[0,2]-m[2,0])/s])
    s = math.sqrt(1 + m[2,2] - m[0,0] - m[1,1])*2; return np.array([(m[0,2]+m[2,0])/s, (m[1,2]+m[2,1])/s, s/4, (m[1,0]-m[0,1])/s])
def angle(a, b):
    d = abs(float(np.dot(a, b))); return math.degrees(2*math.acos(min(1.0, d)))
def rotvec_deg(q):
    if q[3] < 0: q = -q
    s = np.linalg.norm(q[:3]); a = 2*math.atan2(s, q[3])
    return np.zeros(3) if s < 1e-12 else q[:3]/s*math.degrees(a)

def chatter(ts, qs, eyes, speeds):
    wl = np.zeros(3); vl = np.zeros(3); w2 = 0; wa = np.zeros(3); v2 = 0; n = 0
    for i in range(1, len(ts)):
        dt = ts[i] - ts[i-1]
        if dt <= 0: continue
        w = rotvec_deg(qmul(qs[i], qinv(qs[i-1])))/dt; v = (eyes[i] - eyes[i-1])/dt
        k = 1 - math.exp(-dt/0.05); wl += (w - wl)*k; vl += (v - vl)*k
        if speeds[i] > 15:
            r = w - wl; rl = qrot(qinv(qs[i]), r); w2 += r @ r; wa += rl*rl; v2 += (v - vl) @ (v - vl); n += 1
    n = max(n, 1)
    return math.sqrt(w2/n), np.sqrt(wa/n), math.sqrt(v2/n), n

def run(path):
    op = gzip.open if path.endswith(".gz") else open
    rows = [r for r in csv.DictReader(op(path, "rt")) if r["active"] == "1"]
    f = lambda r, k: float(r[k])
    ts = [f(r, "pres") for r in rows]; speeds = [f(r, "speed") for r in rows]
    qs = [np.array([f(r, "cqx"), f(r, "cqy"), f(r, "cqz"), f(r, "cqw")]) for r in rows]
    bqs = [np.array([f(r, "bqx"), f(r, "bqy"), f(r, "bqz"), f(r, "bqw")]) for r in rows]
    eyes = [qrot(qinv(bq), np.array([f(r, "cx"), f(r, "cy"), f(r, "cz")]) - np.array([f(r, "bx"), f(r, "by"), f(r, "bz")])) for r, bq in zip(rows, bqs)]
    # VIEW SMOOTHING on the frame's up
    lean = 0.0; up = None; upv = np.zeros(3); q2 = []
    for i, r in enumerate(rows):
        dt = ts[i] - ts[i-1] if i else 0.0
        raw = max(-1.0, min(1.0, f(r, "in7"))); lean = raw if i == 0 else lean + max(-3.5*dt, min(3.5*dt, raw - lean))
        look = PITCH*20 + 15 + (lean*10 if lean > 0 else 0)
        frame = qmul(qs[i], axis_angle((1, 0, 0), -look)); fu = qrot(frame, np.array([0, 1.0, 0])); ff = qrot(frame, np.array([0, 0, 1.0]))
        if up is None or dt > 0.25 or dt < 0: up = fu.copy(); upv[:] = 0
        elif dt > 0:
            w = 1/TAU_VIEW; k = math.exp(-w*dt); e = up - fu; c = upv + w*e; up = fu + (e + c*dt)*k; upv = (upv - w*dt*c)*k
        u = up/np.linalg.norm(up); fp = ff - u*np.dot(ff, u)
        q2.append(qmul(look_rotation(fp, u), axis_angle((1, 0, 0), look)))
    # HEAD SMOOTHING: ((1 + s a)/(1 + s b))^2 = (r + (1 - r) LP_b)^2, r = a / b
    rr = TAU_EYE_OLD/TAU_EYE; x = np.array(eyes); y = x
    for _ in range(2):
        lp = y[0].copy(); out = [y[0].copy()]
        for i in range(1, len(y)):
            dt = ts[i] - ts[i-1]
            if dt > 0: lp = y[i] + (lp - y[i])*math.exp(-dt/TAU_EYE)
            out.append(rr*y[i] + (1 - rr)*lp)
        y = np.array(out)
    eyes2 = list(y)
    dang = sorted(angle(a, b) for a, b in zip(qs, q2)); dpos = sorted(float(np.linalg.norm(a - b)) for a, b in zip(eyes, eyes2))
    P = lambda a, p: a[min(len(a)-1, int(p*(len(a)-1)))]
    dts = [b - a for a, b in zip(ts, ts[1:]) if b > a]
    print(f"{path}: {len(rows)} frames, {1/np.median(dts):.0f} fps (presentation clock)")
    for name, qq, ee in (("old (as logged)", qs, eyes), (f"VIEW {TAU_VIEW:g} only", q2, eyes), (f"HEAD {TAU_EYE:g} only", qs, eyes2), ("both", q2, eyes2)):
        w, wa, v, n = chatter(ts, qq, ee, speeds)
        print(f"  {name:16s} view chatter {w:5.2f} deg/s (pitch {wa[0]:5.2f}, yaw {wa[1]:5.2f}, roll {wa[2]:5.2f}), eye {v:.3f} ft/s  [{n} frames over 15 mph]")
    print(f"  tweaked vs logged: view angle median {P(dang,.5):.3f} p95 {P(dang,.95):.3f} max {P(dang,1):.3f} deg; eye median {P(dpos,.5):.4f} p95 {P(dpos,.95):.4f} max {P(dpos,1):.4f} ft")

for p in sys.argv[1:]: run(p)

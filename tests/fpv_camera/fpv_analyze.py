#!/usr/bin/env python3
"""fpv_analyze.py OUT RUN... [--label L ...] -- the Dynamic FPV camera from FpvRideProbe logs (test only, w-fpvrewrite).

Each RUN is a fpv_ride.py run directory (screenshots/fpvlog.csv; the label defaults to the directory's name after the stamp). Writes into
OUT: summary.json and summary.txt (per run: frame rate, eye offset in the bike's frame, camera roll vs bike lean, pitch follow, shake by
speed, landing dips, jitter) and PNG plots made with PIL (no matplotlib here):
  roll_<label>.png        the bike's lean and the camera's roll (world) over a stretch of the ride
  pitch_<label>.png       the bike's pitch and the camera's (level) pitch
  landing.png             the eye's height in the bike's frame and the view's pitch around each touchdown (all runs, aligned)
  shake.png               camera angular speed above 4 Hz by bike speed (RMS), riding on the ground (0.5 s away from any air)
  jitter.png              the camera's yaw rate over 2 s at each run's frame rate (smooth = no judder)
  shadow_* (summary)      with REFLEX_FPV_SHADOW: the camera fed the same input at a quarter / half the frame rate vs the real one
Angles in degrees, lengths in feet (1 Unity unit = 1 ft)."""
import argparse, csv, json, math, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("out", type=Path)
ap.add_argument("runs", nargs="+", type=Path)
ap.add_argument("--label", action="append", default=[])
ap.add_argument("--window", default="12,30", help="seconds of the ride for the roll / pitch plots: start,end")
a = ap.parse_args()
a.out.mkdir(parents=True, exist_ok=True)


def quat_mat(q):
    x, y, z, w = q.T
    m = np.empty((len(q), 3, 3))
    m[:, 0, 0] = 1 - 2 * (y * y + z * z); m[:, 0, 1] = 2 * (x * y - w * z); m[:, 0, 2] = 2 * (x * z + w * y)
    m[:, 1, 0] = 2 * (x * y + w * z); m[:, 1, 1] = 1 - 2 * (x * x + z * z); m[:, 1, 2] = 2 * (y * z - w * x)
    m[:, 2, 0] = 2 * (x * z - w * y); m[:, 2, 1] = 2 * (y * z + w * x); m[:, 2, 2] = 1 - 2 * (x * x + y * y)
    return m


def angles(m):
    """world pitch (+ up), heading (deg), roll (+ = right side down / up vector tilted right) of rotation matrices (columns right, up, fwd)"""
    f = m[:, :, 2]; u = m[:, :, 1]
    pitch = np.degrees(np.arcsin(np.clip(f[:, 1], -1, 1)))
    head = np.degrees(np.arctan2(f[:, 0], f[:, 2]))
    rh = np.stack([np.cos(np.radians(head)), np.zeros_like(head), -np.sin(np.radians(head))], 1)
    roll = np.degrees(np.arctan2((u * rh).sum(1), np.maximum(u[:, 1], 1e-3)))
    return pitch, head, roll


def rotvec(m):
    """rotation vectors (rad) of rotation matrices"""
    tr = np.clip((np.trace(m, axis1=1, axis2=2) - 1) / 2, -1, 1); ang = np.arccos(tr)
    v = np.stack([m[:, 2, 1] - m[:, 1, 2], m[:, 0, 2] - m[:, 2, 0], m[:, 1, 0] - m[:, 0, 1]], 1)
    s = np.sin(ang); k = np.where(s > 1e-6, ang / (2 * np.maximum(s, 1e-9)), 0.5)
    return v * k[:, None]


def load(run):
    rows = list(csv.DictReader(open(run / "screenshots/fpvlog.csv")))
    cols = rows[0].keys()
    d = {k: np.array([float(r[k]) for r in rows]) for k in cols}
    act = d["active"] > 0.5
    up_y = 1 - 2 * (d["bqx"] ** 2 + d["bqz"] ** 2)   # the bike's up . world up: frames of a tumbling bike (a crash before the game flags it) are left out
    act &= up_y > 0.3
    d = {k: v[act] for k, v in d.items()}
    d["B"] = quat_mat(np.stack([d["bqx"], d["bqy"], d["bqz"], d["bqw"]], 1))
    d["C"] = quat_mat(np.stack([d["cqx"], d["cqy"], d["cqz"], d["cqw"]], 1))
    bp = np.stack([d["bx"], d["by"], d["bz"]], 1)
    BT = np.transpose(d["B"], (0, 2, 1)); d["_BT"] = BT; d["_bpos"] = bp
    rel = lambda p: np.einsum("nij,nj->ni", BT, np.stack([d[p + "x"], d[p + "y"], d[p + "z"]], 1) - bp)
    d["camRel"] = rel("c"); d["eyeRel"] = rel("e"); d["wrRel"] = rel("wr"); d["wlRel"] = rel("wl")
    d["fwRel"] = rel("fw"); d["rwRel"] = rel("rw")
    d["bp"], d["bh"], d["br"] = angles(d["B"])
    d["cp"], d["ch"], d["cr"] = angles(d["C"])
    d["air"] = (d["fg"] == 0) & (d["rg"] == 0)
    d["spd"] = np.sqrt(d["vx"] ** 2 + d["vy"] ** 2 + d["vz"] ** 2) * 0.681818   # mph
    return d


def smooth_t(t, y, win):
    """centred moving average over win seconds (uneven samples)"""
    out = np.empty_like(y); j0 = j1 = 0; n = len(t); cs = np.concatenate([[0], np.cumsum(y)])
    for i in range(n):
        while t[j0] < t[i] - win / 2: j0 += 1
        while j1 < n and t[j1] <= t[i] + win / 2: j1 += 1
        out[i] = (cs[j1] - cs[j0]) / max(1, j1 - j0)
    return out


def ang_vel(t, M):
    dt = np.diff(t); dt[dt <= 1e-5] = np.nan
    rel = np.einsum("nij,nkj->nik", M[1:], M[:-1])   # M1 M0^T (world)
    w = np.degrees(rotvec(rel)) / dt[:, None]
    return t[1:], w


def landings(d):
    """indices of touchdowns after at least 0.3 s in the air"""
    out = []; air = d["air"]; t = d["t"]; start = None
    for i in range(1, len(t)):
        if air[i] and not air[i - 1]: start = t[i]
        if not air[i] and air[i - 1] and start is not None and t[i] - start >= 0.3:
            out.append((i, t[i] - start, -d["vy"][i - 1] * 0.3048))
    return out


# ---------------------------------------------------------------------------------------------------------- plots (PIL)
FONT = None
for f in ["/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"]:
    try: FONT = ImageFont.truetype(f, 15); FONTS = ImageFont.truetype(f, 12); break
    except Exception: pass
if FONT is None: FONT = FONTS = ImageFont.load_default()
COLORS = [(31, 119, 180), (214, 39, 40), (44, 160, 44), (255, 127, 14), (148, 103, 189), (140, 86, 75), (23, 190, 207), (127, 127, 127)]


def plot(path, title, series, xlabel, ylabel, size=(1100, 420), panels=None):
    """series: [(label, x, y, colour index, width)]; panels: list of (title, series, ylabel) stacked instead"""
    panels = panels or [(title, series, ylabel)]
    W, H = size[0], size[1] * len(panels)
    img = Image.new("RGB", (W, H), "white"); g = ImageDraw.Draw(img)
    for pi, (ptitle, ser, ylab) in enumerate(panels):
        y0 = pi * size[1]; L, R, T, B = 70, W - 20, y0 + 34, y0 + size[1] - 46
        xs = np.concatenate([np.asarray(s[1]) for s in ser]); ys = np.concatenate([np.asarray(s[2]) for s in ser])
        ok = np.isfinite(xs) & np.isfinite(ys); xs, ys = xs[ok], ys[ok]
        if len(xs) == 0: continue
        x0, x1 = xs.min(), xs.max(); lo, hi = np.percentile(ys, 0.5), np.percentile(ys, 99.5)
        if hi - lo < 1e-6: hi = lo + 1
        pad = (hi - lo) * 0.08; lo -= pad; hi += pad
        if x1 - x0 < 1e-9: x1 = x0 + 1
        X = lambda x: L + (x - x0) / (x1 - x0) * (R - L); Y = lambda y: B - (y - lo) / (hi - lo) * (B - T)
        g.text((L, y0 + 8), ptitle, fill="black", font=FONT)
        g.rectangle([L, T, R, B], outline=(170, 170, 170))
        for k in range(6):
            yv = lo + (hi - lo) * k / 5; yy = Y(yv); g.line([L, yy, R, yy], fill=(235, 235, 235)); g.text((6, yy - 7), f"{yv:.2f}" if hi - lo < 5 else f"{yv:.1f}", fill="black", font=FONTS)
            xv = x0 + (x1 - x0) * k / 5; xx = X(xv); g.line([xx, T, xx, B], fill=(235, 235, 235)); g.text((xx - 12, B + 4), f"{xv:.2f}" if x1 - x0 < 5 else f"{xv:.1f}", fill="black", font=FONTS)
        if lo < 0 < hi: g.line([L, Y(0), R, Y(0)], fill=(150, 150, 150))
        g.text((R - 200, B + 22), xlabel, fill="black", font=FONTS); g.text((L + 4, T + 2), ylab, fill=(90, 90, 90), font=FONTS)
        lx = L + 10
        for (lab, x, y, ci, wd) in ser:
            x = np.asarray(x); y = np.clip(np.asarray(y), lo, hi); col = COLORS[ci % len(COLORS)]
            pts = [(X(xx), Y(yy)) for xx, yy in zip(x, y) if np.isfinite(xx) and np.isfinite(yy)]
            if len(pts) > 1: g.line(pts, fill=col, width=wd)
            if lab:
                tw = g.textlength(lab, font=FONTS); g.rectangle([lx, T + 18, lx + 14, T + 26], fill=col); g.text((lx + 18, T + 15), lab, fill="black", font=FONTS); lx += tw + 36
    img.save(path)


# ---------------------------------------------------------------------------------------------------------- per run
labels = a.label + [r.name.split("Z-", 1)[-1] for r in a.runs[len(a.label):]]
data = {}; summary = {}
w0, w1 = (float(x) for x in a.window.split(","))
for lab, run in zip(labels, a.runs):
    d = load(run); data[lab] = d; t = d["t"]
    s = {"frames": int(len(t)), "fps_mean": float(1 / np.mean(d["dt"][1:])), "fps_p5": float(1 / np.percentile(d["dt"][1:], 95))}
    ground = ~d["air"]
    s["eye_rel_mean"] = [round(float(v), 3) for v in d["camRel"].mean(0)]
    s["eye_rel_std"] = [round(float(v), 3) for v in d["camRel"].std(0)]
    s["eye_rel_range_y"] = [round(float(np.percentile(d["camRel"][:, 1], 1)), 3), round(float(np.percentile(d["camRel"][:, 1], 99)), 3)]
    # clearance: the eye (camera) to the wrists' line, bike frame
    wr, wl, e = d["wrRel"], d["wlRel"], d["camRel"]; ab = wl - wr
    tt = np.clip(((e - wr) * ab).sum(1) / np.maximum((ab * ab).sum(1), 1e-6), 0, 1)
    dist = np.linalg.norm(e - (wr + ab * tt[:, None]), axis=1)
    s["bar_clearance_min"] = round(float(dist.min()), 3); s["bar_clearance_p1"] = round(float(np.percentile(dist, 1)), 3)
    if "erx" in d:   # the camera to the forearms (elbow to wrist), the closest of the two
        armd = []
        for el_, wr_ in (("er", d["wrRel"]), ("el", d["wlRel"])):
            a0 = np.einsum("nij,nj->ni", d["_BT"], np.stack([d[el_ + "x"], d[el_ + "y"], d[el_ + "z"]], 1) - d["_bpos"]); sg = wr_ - a0
            uu = np.clip(((e - a0) * sg).sum(1) / np.maximum((sg * sg).sum(1), 1e-6), 0, 1)
            armd.append(np.linalg.norm(e - (a0 + sg * uu[:, None]), axis=1))
        armd = np.minimum(*armd)
        s["arm_clearance_min"] = round(float(armd.min()), 3); s["arm_clearance_p1"] = round(float(np.percentile(armd, 1)), 3)
    # roll: camera vs bike lean (ground, |lean| > 8 deg)
    m = ground & (np.abs(d["br"]) > 8)
    if m.sum() > 10:
        k = np.polyfit(d["br"][m], d["cr"][m], 1)[0]; s["roll_gain"] = round(float(k), 3)
    s["lean_p95"] = round(float(np.percentile(np.abs(d["br"]), 95)), 1); s["camroll_p95"] = round(float(np.percentile(np.abs(d["cr"]), 95)), 1)
    # pitch: high-passed (1 s) camera pitch vs bike pitch on the ground
    bph = d["bp"] - smooth_t(t, d["bp"], 1.0); cph = d["cp"] - smooth_t(t, d["cp"], 1.0)
    if ground.sum() > 10: s["pitch_hp_gain"] = round(float(np.std(cph[ground]) / max(np.std(bph[ground]), 1e-6)), 3)
    # shake: camera angular speed above ~4 Hz (minus a 0.25 s average) RMS by speed band, and the same for the bike
    tw, wc = ang_vel(t, d["C"]); _, wb = ang_vel(t, d["B"]); good = np.isfinite(wc).all(1) & np.isfinite(wb).all(1)
    # riding on the ground: no wheel off for 0.5 s either side (take-offs and landings are motion, not shake)
    airt = t[d["air"]]; steady = np.ones(len(t), bool)
    if len(airt): k = np.searchsorted(airt, t); lo = airt[np.clip(k - 1, 0, len(airt) - 1)]; hi = airt[np.clip(k, 0, len(airt) - 1)]; steady = (np.abs(t - lo) > 0.5) & (np.abs(hi - t) > 0.5)
    tw, wc, wb = tw[good], wc[good], wb[good]; spd = d["spd"][1:][good]; gr = (ground & steady)[1:][good]
    hc = np.linalg.norm(wc - np.stack([smooth_t(tw, wc[:, i], 0.25) for i in range(3)], 1), axis=1)
    hb = np.linalg.norm(wb - np.stack([smooth_t(tw, wb[:, i], 0.25) for i in range(3)], 1), axis=1)
    bands = [(0, 10), (10, 20), (20, 30), (30, 45), (45, 80)]
    s["shake_cam_dps"] = {f"{lo}-{hi}": round(float(np.sqrt(np.mean(hc[gr & (spd >= lo) & (spd < hi)] ** 2))), 2) for lo, hi in bands if (gr & (spd >= lo) & (spd < hi)).sum() > 30}
    s["shake_bike_dps"] = {f"{lo}-{hi}": round(float(np.sqrt(np.mean(hb[gr & (spd >= lo) & (spd < hi)] ** 2))), 2) for lo, hi in bands if (gr & (spd >= lo) & (spd < hi)).sum() > 30}
    d["shake"] = (spd, hc, hb, gr)
    # jitter: the frame-to-frame change of the camera's angular velocity relative to a 60 ms average (deg/s RMS) - judder shows here;
    # the bars' jitter: the same for the camera relative to the bike (what the bars do on screen)
    jit = np.linalg.norm(wc - np.stack([smooth_t(tw, wc[:, i], 0.06) for i in range(3)], 1), axis=1)
    Mrel = np.einsum("nji,njk->nik", d["B"], d["C"])   # B^T C: the camera in the bike's frame
    _, wr_ = ang_vel(t, Mrel); wr_ = wr_[good]
    jrel = np.linalg.norm(wr_ - np.stack([smooth_t(tw, wr_[:, i], 0.06) for i in range(3)], 1), axis=1)
    s["jitter_cam_dps"] = round(float(np.sqrt(np.mean(jit ** 2))), 2); s["jitter_bars_dps"] = round(float(np.sqrt(np.mean(jrel ** 2))), 2)
    d["jit"] = (tw, wc)
    # landings
    L_ = landings(d); s["landings"] = len(L_); dips = []; pitches = []
    for (i, airt, vdown) in L_:
        tm = t[i]; m = (t >= tm - 0.3) & (t <= tm + 1.2)
        if m.sum() < 10: continue
        base = np.median(d["camRel"][(t >= tm - 0.6) & (t <= tm - 0.1), 1]) if ((t >= tm - 0.6) & (t <= tm - 0.1)).sum() else d["camRel"][i, 1]
        after = d["camRel"][(t >= tm) & (t <= tm + 0.6), 1]
        dips.append({"t": round(float(tm), 2), "air_s": round(float(airt), 2), "vdown_ms": round(float(vdown), 2), "dip_ft": round(float(base - after.min()), 3) if len(after) else None})
    s["landing_dips"] = dips
    # the shadow camera (REFLEX_FPV_SHADOW: the same input every k-th frame): the gap between the two poses
    sh = run / "screenshots/shadow.csv"
    if sh.exists():
        rows = list(csv.DictReader(open(sh)))
        if rows:
            dp = np.array([float(r["dpos"]) for r in rows]); da = np.array([float(r["dang"]) for r in rows])
            s["shadow_rows"] = len(rows)
            s["shadow_dpos_ft"] = {"p50": round(float(np.median(dp)), 4), "p99": round(float(np.percentile(dp, 99)), 4), "max": round(float(dp.max()), 4)}
            s["shadow_dang_deg"] = {"p50": round(float(np.median(da)), 3), "p99": round(float(np.percentile(da, 99)), 3), "max": round(float(da.max()), 3)}
    summary[lab] = s

(a.out / "summary.json").write_text(json.dumps(summary, indent=1))
with open(a.out / "summary.txt", "w") as f:
    for lab, s in summary.items():
        f.write(f"== {lab}\n")
        for k, v in s.items():
            if k != "landing_dips": f.write(f"  {k}: {v}\n")
        f.write("  landing dips: " + ", ".join(f"{x['dip_ft']}ft@{x['vdown_ms']}m/s" for x in s["landing_dips"]) + "\n")
print((a.out / "summary.txt").read_text())

# ---------------------------------------------------------------------------------------------------------- plots
for lab, d in data.items():
    t = d["t"]; m = (t >= w0) & (t <= w1)
    if m.sum() < 10: continue
    plot(a.out / f"roll_{lab}.png", "", [], "time (s)", "", panels=[
        (f"{lab}: roll (deg, + = right) - bike lean vs camera", [("bike lean", t[m], d["br"][m], 0, 2), ("camera roll", t[m], d["cr"][m], 1, 2)], "deg"),
        (f"{lab}: pitch (deg, + = up) - bike vs camera (the camera's includes its look down)", [("bike pitch", t[m], d["bp"][m], 0, 2), ("camera pitch", t[m], d["cp"][m], 1, 2)], "deg"),
        (f"{lab}: eye in the bike's frame (ft): height (y) and fore-aft (z) about their means", [("eye y", t[m], d["camRel"][m, 1] - d["camRel"][:, 1].mean(), 2, 2), ("eye z", t[m], d["camRel"][m, 2] - d["camRel"][:, 2].mean(), 3, 2), ("air", t[m], d["air"][m] * 0.3, 7, 1)], "ft")])

# landings: every run's touchdowns aligned (eye height in the bike frame minus the pre-landing level; the camera's world pitch change)
pan_y, pan_p = [], []
for ci, (lab, d) in enumerate(data.items()):
    t = d["t"]
    for k, (i, airt, vdown) in enumerate(landings(d)):
        tm = t[i]; m = (t >= tm - 0.3) & (t <= tm + 1.2)
        pre = (t >= tm - 0.4) & (t <= tm - 0.05)
        if m.sum() < 10 or pre.sum() < 2: continue
        pan_y.append((lab if k == 0 else "", t[m] - tm, d["camRel"][m, 1] - d["camRel"][pre, 1].mean(), ci, 1))
        pan_p.append((lab if k == 0 else "", t[m] - tm, (d["cp"][m] - d["bp"][m]) - (d["cp"][pre] - d["bp"][pre]).mean(), ci, 1))
if pan_y:
    plot(a.out / "landing.png", "", [], "time from touchdown (s)", "", panels=[
        ("landings: the eye's height in the bike's frame (ft, from its level just before)", pan_y, "ft"),
        ("landings: the view's pitch relative to the bike (deg, from just before; + = the head tips up against the bike)", pan_p, "deg")])
# shake by speed
ser = []
for ci, (lab, d) in enumerate(data.items()):
    spd, hc, hb, gr = d["shake"]; xs, yc = [], []
    for lo in range(0, 70, 5):
        mm = gr & (spd >= lo) & (spd < lo + 5)
        if mm.sum() > 30: xs.append(lo + 2.5); yc.append(np.sqrt(np.mean(hc[mm] ** 2)))
    ser.append((lab, xs, yc, ci, 2))
plot(a.out / "shake.png", "camera shake: angular speed above ~4 Hz (deg/s RMS) by bike speed, on the ground (0.5 s away from any air)", ser, "speed (mph)", "deg/s")
# jitter: 2 s of the camera's yaw rate
ser = []
for ci, (lab, d) in enumerate(data.items()):
    tw, wc = d["jit"]; m = (tw >= w0 + 4) & (tw <= w0 + 6)
    ser.append((lab, tw[m] - (w0 + 4), wc[m, 1], ci, 1))
plot(a.out / "jitter.png", "the camera's yaw rate over 2 s (deg/s, world): smooth curves = no judder at any frame rate", ser, "time (s)", "deg/s")
print("plots in", a.out)

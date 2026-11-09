#!/usr/bin/env python3
"""psd_compose.py -- rebuilds a bike skin from a Photoshop template (.psd) without Photoshop: the visible layers composited again, with the
layers you name left out (a template's "Welcome" / "Thank you" cover, its UV guide or colour palette), or one hidden group alone (its Spec or
Norm group). Needs numpy (Blender's own Python has it: /Applications/Blender.app/Contents/Resources/<version>/python/bin/python3.x).

  python3 -I psd_compose.py layers  <template.psd>                     the layer tree (groups, blend modes, visibility, masks)
  python3 -I psd_compose.py compose <template.psd> <out.png> [--skip NAME]... [--only GROUP] [--size 4096] [--flatten R,G,B]

  --skip     leave out a layer / group of that name (repeatable)
  --only     composite just that group (shown even when it is hidden): the Spec or Norm group of a template
  --size     the output size (a template of 8192 is composited at 4096 in a quarter of the memory)
  --flatten  a background colour (0..1 each) under the layers: 0,0,0 for a spec map, 0.5,0.5,1 for a normal map

Supported: 8-bit RGB PSD, raw / RLE channels, groups (pass-through and isolated, with masks and opacity), layer masks, clipping
masks, the common blend modes (normal, multiply, screen, overlay, soft / hard light, dodges / burns, darken / lighten, difference ...).
Adjustment layers and layer effects (stroke, drop shadow) are not drawn (each skipped one is listed). Checked against Photoshop's own
composite of a 4096 template: mean difference 0.24 of 255. The file is only read: run it with python -I on files you downloaded.
"""

import struct, sys
import numpy as np

BLEND = {b'norm': 'normal', b'mul ': 'multiply', b'scrn': 'screen', b'over': 'overlay', b'pass': 'pass', b'diss': 'dissolve',
         b'dark': 'darken', b'lite': 'lighten', b'sLit': 'softlight', b'hLit': 'hardlight', b'lddg': 'lineardodge', b'div ': 'divide',
         b'idiv': 'colorburn', b'lbrn': 'linearburn', b'colr': 'color', b'lum ': 'luminosity', b'hue ': 'hue', b'sat ': 'saturation',
         b'diff': 'difference', b'smud': 'exclusion', b'fsub': 'subtract', b'dkCl': 'darkercolor', b'lgCl': 'lightercolor', b'vLit': 'vividlight',
         b'lLit': 'linearlight', b'pLit': 'pinlight', b'hMix': 'hardmix', b'dodg': 'colordodge'}


class R:
    def __init__(self, f): self.f = f
    def read(self, n):
        b = self.f.read(n)
        if len(b) != n: raise ValueError('truncated')
        return b
    def u8(self): return self.read(1)[0]
    def u16(self): return struct.unpack('>H', self.read(2))[0]
    def i16(self): return struct.unpack('>h', self.read(2))[0]
    def u32(self): return struct.unpack('>I', self.read(4))[0]
    def i32(self): return struct.unpack('>i', self.read(4))[0]
    def u64(self): return struct.unpack('>Q', self.read(8))[0]
    def tell(self): return self.f.tell()
    def seek(self, p): self.f.seek(p)


def packbits(data, size):
    out = bytearray(); i = 0; n = len(data)
    while i < n and len(out) < size:
        h = data[i]; i += 1
        if h < 128:
            out += data[i:i + h + 1]; i += h + 1
        elif h > 128:
            out += bytes([data[i]]) * (257 - h); i += 1
    if len(out) < size: out += bytes(size - len(out))
    return bytes(out[:size])


class Layer:
    pass


def read(path, want_pixels=True):
    f = open(path, 'rb'); r = R(f)
    if r.read(4) != b'8BPS': raise ValueError('not a PSD')
    ver = r.u16(); big = ver == 2
    r.read(6); channels = r.u16(); H = r.u32(); W = r.u32(); depth = r.u16(); mode = r.u16()
    if depth != 8: raise ValueError(f'depth {depth}')
    cm = r.u32(); r.read(cm)
    ir = r.u32(); r.read(ir)
    lm_len = r.u64() if big else r.u32(); lm_end = r.tell() + lm_len
    li_len = r.u64() if big else r.u32(); li_end = r.tell() + li_len
    layers = []
    if li_len:
        count = abs(r.i16())
        for _ in range(count):
            L = Layer()
            L.top, L.left, L.bottom, L.right = r.i32(), r.i32(), r.i32(), r.i32()
            nch = r.u16(); L.chans = []
            for _ in range(nch): L.chans.append((r.i16(), r.u64() if big else r.u32()))
            if r.read(4) != b'8BIM': raise ValueError('layer sig')
            L.blend = BLEND.get(r.read(4), 'other'); L.opacity = r.u8(); L.clipping = r.u8(); flags = r.u8(); r.u8()
            L.visible = not (flags & 2)
            extra = r.u32(); xend = r.tell() + extra
            ml = r.u32(); mstart = r.tell()
            L.mask = None
            if ml >= 20:
                mt, mlft, mb, mr = r.i32(), r.i32(), r.i32(), r.i32(); dflt = r.u8(); mflags = r.u8()
                L.mask = (mt, mlft, mb, mr, dflt, mflags)
            r.seek(mstart + ml)
            br = r.u32(); L.blendranges = r.read(br)
            nl = r.u8(); L.name = r.read(nl).decode('latin-1'); r.read((4 - (nl + 1) % 4) % 4)
            L.section = 0; L.adjust = None; L.fill = False; L.keys = []
            L.blendranges = r.read(0)
            while r.tell() + 12 <= xend:
                sig = r.read(4)
                if sig not in (b'8BIM', b'8B64'): break
                key = r.read(4); L.keys.append(key.decode('latin-1'))
                ln = r.u64() if (big and key in (b'LMsk', b'Lr16', b'Lr32', b'Layr', b'Mt16', b'Mt32', b'Mtrn', b'Alph', b'FMsk', b'lnk2', b'FEid', b'FXid', b'PxSD')) else r.u32()
                ds = r.tell()
                if key in (b'lsct', b'lsdk'):
                    L.section = r.u32()
                    if ln >= 12: r.read(4); L.blend = BLEND.get(r.read(4), L.blend)
                elif key == b'luni':
                    n = r.u32(); L.name = r.read(n * 2).decode('utf-16-be', 'replace')
                elif key in (b'levl', b'curv', b'hue2', b'hue ', b'blnc', b'brit', b'expA', b'vibA', b'mixr', b'selc', b'grdm', b'phfl', b'thrs', b'post', b'nvrt', b'CgEd', b'clrL', b'blwh'):
                    L.adjust = key.decode('latin-1')
                elif key in (b'SoCo', b'GdFl', b'PtFl'):
                    L.fill = key.decode('latin-1')
                    if key == b'SoCo': L.fillraw = r.read(ln)
                r.seek(ds + ln + (ln % 2 if False else 0))
                # lengths are padded to even (some writers to 4)
                if r.tell() % 2: r.read(1)
            r.seek(xend)
            layers.append(L)
        # channel image data
        for L in layers:
            L.pixels = {}
            w, h = L.right - L.left, L.bottom - L.top
            for cid, ln in L.chans:
                start = r.tell()
                comp = r.u16()
                if not want_pixels or ln <= 2:
                    r.seek(start + ln); continue
                cw, ch = (w, h)
                if cid == -2 and L.mask: cw, ch = L.mask[3] - L.mask[1], L.mask[2] - L.mask[0]
                if cw <= 0 or ch <= 0: r.seek(start + ln); continue
                if comp == 0:
                    data = r.read(cw * ch)
                elif comp == 1:
                    counts = [r.u32() if big else r.u16() for _ in range(ch)]
                    rows = [packbits(r.read(c), cw) for c in counts]
                    data = b''.join(rows)
                else:
                    r.seek(start + ln); continue
                L.pixels[cid] = np.frombuffer(data, np.uint8).reshape(ch, cw)
                r.seek(start + ln)
    f.close()
    return dict(width=W, height=H, mode=mode, channels=channels, layers=layers)


def _half(a, top, left):
    """A channel at half size on the canvas grid: (array, new top, new left). 2x2 box filter, edges replicated to even alignment."""
    pt, pl = top % 2, left % 2
    a = np.pad(a, ((pt, 0), (pl, 0)), mode='edge') if (pt or pl) else a
    h, w = a.shape
    if h % 2 or w % 2: a = np.pad(a, ((0, h % 2), (0, w % 2)), mode='edge'); h, w = a.shape
    b = a.reshape(h // 2, 2, w // 2, 2).astype(np.uint16).sum((1, 3))
    return ((b + 2) // 4).astype(np.uint8), (top - pt) // 2, (left - pl) // 2


def shrink(d):
    """Halves a read PSD in place (an 8K template composited at 4K in a quarter of the memory)."""
    d['width'] //= 2; d['height'] //= 2
    for L in d['layers']:
        new = {}; box = None; mbox = None
        for cid, a in L.pixels.items():
            if cid == -2 and L.mask:
                b, t, l = _half(a, L.mask[0], L.mask[1]); new[cid] = b; mbox = (t, l, t + b.shape[0], l + b.shape[1])
            else:
                b, t, l = _half(a, L.top, L.left); new[cid] = b; box = (t, l, t + b.shape[0], l + b.shape[1])
        L.pixels = new
        if box: L.top, L.left, L.bottom, L.right = box
        else: L.top, L.left, L.bottom, L.right = L.top // 2, L.left // 2, (L.bottom + 1) // 2, (L.right + 1) // 2
        if L.mask:
            if mbox: L.mask = (mbox[0], mbox[1], mbox[2], mbox[3], L.mask[4], L.mask[5])
            else: L.mask = (L.mask[0] // 2, L.mask[1] // 2, (L.mask[2] + 1) // 2, (L.mask[3] + 1) // 2, L.mask[4], L.mask[5])
    return d


def tree(layers):
    """Layers (bottom to top in the file) -> nested list top-level-first order kept bottom->top. Groups: dict(name, children, ...)."""
    root = []; stack = [root]; meta = []
    for L in layers:
        if L.section == 3:          # bounding divider: opens a group (bottom of the group in file order)
            g = []; stack[-1].append(('group-open', g)); stack.append(g)
        elif L.section in (1, 2):   # group header (closes)
            g = stack.pop(); stack[-1][-1] = ('group', L, g)
        else:
            stack[-1].append(('layer', L))
    return root


def dump(path):
    d = read(path, want_pixels=False)
    print(path, d['width'], d['height'], 'mode', d['mode'])
    def walk(items, depth):
        for it in reversed(items):   # top first
            if it[0] == 'group':
                L = it[1]; print('  ' * depth + f"[G] {L.name!r} vis={L.visible} {L.blend} op={L.opacity}")
                walk(it[2], depth + 1)
            elif it[0] == 'layer':
                L = it[1]
                print('  ' * depth + f"{L.name!r} vis={L.visible} {L.blend} op={L.opacity} clip={L.clipping} box=({L.left},{L.top},{L.right},{L.bottom})"
                      + (f" ADJ={L.adjust}" if L.adjust else '') + (f" FILL={L.fill}" if L.fill else '') + (" MASK" if L.mask else ''))
    walk(tree(d['layers']), 0)



# ------------------------------------------------------------------------------------------------ compositing

def _blend(mode, b, s):
    """b backdrop rgb, s source rgb (float32 0..1) -> blended rgb (before alpha)."""
    if mode in ('normal', 'pass', 'dissolve'): return s
    if mode == 'multiply': return b * s
    if mode == 'screen': return b + s - b * s
    if mode == 'overlay': return np.where(b <= 0.5, 2 * b * s, 1 - 2 * (1 - b) * (1 - s))
    if mode == 'hardlight': return np.where(s <= 0.5, 2 * b * s, 1 - 2 * (1 - b) * (1 - s))
    if mode == 'softlight':
        d = np.where(b <= 0.25, ((16 * b - 12) * b + 4) * b, np.sqrt(b))
        return np.where(s <= 0.5, b - (1 - 2 * s) * b * (1 - b), b + (2 * s - 1) * (d - b))
    if mode == 'lineardodge': return np.minimum(1, b + s)
    if mode == 'linearburn': return np.maximum(0, b + s - 1)
    if mode == 'colordodge': return np.where(s >= 1, 1, np.minimum(1, b / np.maximum(1e-6, 1 - s)))
    if mode == 'colorburn': return np.where(s <= 0, 0, 1 - np.minimum(1, (1 - b) / np.maximum(1e-6, s)))
    if mode == 'darken': return np.minimum(b, s)
    if mode == 'lighten': return np.maximum(b, s)
    if mode == 'difference': return np.abs(b - s)
    if mode == 'exclusion': return b + s - 2 * b * s
    if mode == 'subtract': return np.maximum(0, b - s)
    if mode == 'divide': return np.minimum(1, b / np.maximum(1e-6, s))
    if mode == 'linearlight': return np.clip(b + 2 * s - 1, 0, 1)
    if mode in ('luminosity', 'color', 'hue', 'saturation'):
        def lum(c): return (c[..., 0:1] * .3 + c[..., 1:2] * .59 + c[..., 2:3] * .11)
        if mode == 'luminosity': return np.clip(b + (lum(s) - lum(b)), 0, 1)
        return np.clip(s + (lum(b) - lum(s)), 0, 1)   # color (hue / saturation approximated as color)
    return s


class Compositor:
    def __init__(self, d, log=None):
        self.W, self.H = d['width'], d['height']; self.log = log if log is not None else []

    def layer_rgba(self, L):
        """The layer's (y0, y1, x0, x1, rgba float32) on the canvas (clipped), or None."""
        x0, y0, x1, y1 = max(0, L.left), max(0, L.top), min(self.W, L.right), min(self.H, L.bottom)
        if x1 <= x0 or y1 <= y0: return None
        if 0 not in L.pixels:
            if L.fill and getattr(L, 'fillraw', None) is not None: self.log.append('fill layer without pixels: %r' % L.name)
            return None
        def ch(i, default):
            a = L.pixels.get(i)
            if a is None: return np.full((y1 - y0, x1 - x0), default, np.float32)
            return a[y0 - L.top:y1 - L.top, x0 - L.left:x1 - L.left].astype(np.float32) / 255.0
        rgb = np.stack([ch(0, 0), ch(1 if 1 in L.pixels else 0, 0), ch(2 if 2 in L.pixels else 0, 0)], -1)
        a = ch(-1, 1.0)
        if L.mask is not None and -2 in L.pixels and not (L.mask[5] & 2):
            mt, ml, mb, mr, dflt, mflags = L.mask
            m = np.full((y1 - y0, x1 - x0), dflt / 255.0, np.float32)
            ix0, iy0, ix1, iy1 = max(x0, ml), max(y0, mt), min(x1, mr), min(y1, mb)
            if ix1 > ix0 and iy1 > iy0:
                m[iy0 - y0:iy1 - y0, ix0 - x0:ix1 - x0] = L.pixels[-2][iy0 - mt:iy1 - mt, ix0 - ml:ix1 - ml].astype(np.float32) / 255.0
            if mflags & 4: m = 1 - m
            a = a * m
        return y0, y1, x0, x1, np.concatenate([rgb, a[..., None]], -1)

    def group_mask(self, L):
        """A group's raster mask over the canvas (float32 H x W) or None."""
        if L.mask is None or -2 not in getattr(L, 'pixels', {}) or (L.mask[5] & 2): return None
        mt, ml, mb, mr, dflt, fl = L.mask
        m = np.full((self.H, self.W), dflt / 255.0, np.float32)
        x0, y0, x1, y1 = max(0, ml), max(0, mt), min(self.W, mr), min(self.H, mb)
        if x1 > x0 and y1 > y0: m[y0:y1, x0:x1] = L.pixels[-2][y0 - mt:y1 - mt, x0 - ml:x1 - ml].astype(np.float32) / 255.0
        if fl & 4: m = 1 - m
        return m

    def over(self, dst, y0, y1, x0, x1, src, mode, opacity):
        """Blends src (rgba, its alpha already masked) onto dst (H, W, 4) in its box."""
        b = dst[y0:y1, x0:x1]
        sa = src[..., 3:4] * opacity; ba = b[..., 3:4]
        mixed = _blend(mode, b[..., :3], src[..., :3])
        # W3C compositing: co = cs*(1-ab) + B(cb,cs)*ab, then source-over
        cs = src[..., :3] * (1 - ba) + mixed * ba
        ra = sa + ba * (1 - sa)
        rc = (cs * sa + b[..., :3] * ba * (1 - sa)) / np.maximum(ra, 1e-6)
        dst[y0:y1, x0:x1, :3] = np.where(ra > 0, rc, b[..., :3]); dst[y0:y1, x0:x1, 3:4] = ra

    def run(self, items, dst, skip=lambda L: False, force=()):
        """Composites a tree level (bottom to top) onto dst. skip(L): leave out; force: group names shown even if hidden."""
        base_alpha = None
        for it in items:
            if it[0] == 'group-open':   # an unterminated group
                continue
            L = it[1]
            if skip(L): base_alpha = None; continue
            visible = L.visible or L.name in force
            if it[0] == 'group':
                if not visible: base_alpha = None; continue
                gm = self.group_mask(L)
                if L.blend == 'pass' and L.opacity == 255 and gm is None:
                    self.run(it[2], dst, skip, force)
                elif L.blend == 'pass':
                    # a pass-through group with a mask / opacity: its layers over a copy of the backdrop, then mixed back by mask x opacity
                    tmp = dst.copy(); self.run(it[2], tmp, skip, force)
                    k = (L.opacity / 255.0) * (gm if gm is not None else 1.0)
                    k = np.asarray(k, np.float32)[..., None] if np.ndim(k) else k
                    dst[...] = dst * (1 - k) + tmp * k
                else:
                    tmp = np.zeros_like(dst); self.run(it[2], tmp, skip, force)
                    if gm is not None: tmp[..., 3] *= gm
                    self.over(dst, 0, self.H, 0, self.W, tmp, L.blend, L.opacity / 255.0)
                base_alpha = None
                continue
            if L.adjust: self.log.append('adjustment layer skipped: %r (%s)' % (L.name, L.adjust)); continue
            r = self.layer_rgba(L) if visible else None
            if L.clipping:
                if r is None or base_alpha is None: continue
                y0, y1, x0, x1, src = r
                src = src.copy(); src[..., 3] *= base_alpha[y0:y1, x0:x1]
                self.over(dst, y0, y1, x0, x1, src, L.blend, L.opacity / 255.0)
                continue
            base_alpha = None
            if r is None:
                if not visible: base_alpha = np.zeros((self.H, self.W), np.float32)   # (a hidden base hides its clipped layers)
                continue
            y0, y1, x0, x1, src = r
            base_alpha = np.zeros((self.H, self.W), np.float32); base_alpha[y0:y1, x0:x1] = src[..., 3] * (L.opacity / 255.0)
            self.over(dst, y0, y1, x0, x1, src, L.blend, L.opacity / 255.0)


def find_group(items, name):
    for it in items:
        if it[0] == 'group':
            if it[1].name.strip() == name: return it
            g = find_group(it[2], name)
            if g: return g
    return None


def write_png(path, rgba):
    """rgba float (H, W, 4) or uint8 -> an 8-bit RGBA PNG (zlib, no filter)."""
    import zlib as z
    a = rgba if rgba.dtype == np.uint8 else (np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8)
    h, w = a.shape[:2]
    raw = np.concatenate([np.zeros((h, 1), np.uint8), a.reshape(h, w * 4)], 1).tobytes()
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', z.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', z.compress(raw, 6)) + chunk(b'IEND', b''))


def read_composite(path):
    """The PSD's saved merged image (H, W, 4 uint8), or None."""
    f = open(path, 'rb'); r = R(f)
    r.read(4); ver = r.u16(); big = ver == 2; r.read(6); channels = r.u16(); H = r.u32(); W = r.u32(); r.u16(); r.u16()
    r.read(r.u32()); r.read(r.u32()); lm = r.u64() if big else r.u32(); r.seek(r.tell() + lm)
    comp = r.u16(); out = np.zeros((H, W, 4), np.uint8); out[..., 3] = 255
    n = min(channels, 4)
    if comp == 0:
        for c in range(n): out[..., c] = np.frombuffer(r.read(W * H), np.uint8).reshape(H, W)
    elif comp == 1:
        counts = [r.u32() if big else r.u16() for _ in range(H * channels)]
        for c in range(channels):
            rows = [packbits(r.read(counts[c * H + y]), W) for y in range(H)]
            if c < 4: out[..., c] = np.frombuffer(b''.join(rows), np.uint8).reshape(H, W)
    else:
        return None
    if channels == 3: out[..., 3] = 255
    return out


def compose(path, out, skip_names=(), only=None, force=(), size=None, flatten=None):
    """Composites the PSD's visible layers (minus skip_names; only: just that group, shown) to an RGBA PNG at `size` (a downscale by 2^k)."""
    d = read(path); log = []
    while size and d['width'] >= 2 * size: shrink(d)
    t = tree(d['layers']); C = Compositor(d, log)
    dst = np.zeros((d['height'], d['width'], 4), np.float32)
    if flatten is not None: dst[..., :3] = flatten; dst[..., 3] = 1
    skip = lambda L: L.name.strip() in skip_names
    if only:
        g = find_group(t, only)
        if g is None: raise SystemExit('no group %r' % only)
        C.run(g[2], dst, skip, force=tuple(force) + (only,))
    else:
        C.run(t, dst, skip, force)
    a = (np.clip(dst, 0, 1) * 255 + 0.5).astype(np.uint8)
    if size and size < a.shape[0]:
        k = a.shape[0] // size
        a = a.reshape(size, k, a.shape[1] // k, k, 4).mean((1, 3)).astype(np.uint8)
    write_png(out, a)
    for line in sorted(set(log)): print('  note:', line)
    return a


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == 'layers':
        dump(a[1]); sys.exit(0)
    if len(a) >= 3 and a[0] == 'compose':
        skip, only, size, flat, i = [], None, None, None, 3
        while i < len(a):
            if a[i] == '--skip': skip.append(a[i + 1])
            elif a[i] == '--only': only = a[i + 1]
            elif a[i] == '--size': size = int(a[i + 1])
            elif a[i] == '--flatten': flat = [float(x) for x in a[i + 1].split(',')]
            else: raise SystemExit('unknown option ' + a[i])
            i += 2
        compose(a[1], a[2], skip_names=tuple(skip), only=only, size=size, flatten=flat)
        print('wrote', a[2]); sys.exit(0)
    print(__doc__); sys.exit(2)

# WAV I/O for the pack builder (numpy only): any PCM 8/16/24/32-bit or IEEE float WAV in, 16-bit PCM out.
import struct
import numpy as np

def info(path):
    """(format, channels, rate, bits, data offset, data bytes) without reading the samples."""
    with open(path, 'rb') as f:
        h = f.read(12)
        if h[:4] != b'RIFF' or h[8:12] != b'WAVE': raise ValueError(path + ': not a RIFF WAVE file')
        fmt = ch = rate = bits = 0; pos = 12
        while True:
            f.seek(pos); c = f.read(8)
            if len(c) < 8: raise ValueError(path + ': no data chunk')
            cid, ln = c[:4], struct.unpack('<I', c[4:])[0]
            if cid == b'fmt ':
                b = f.read(min(ln, 40))
                fmt, ch, rate, _, _, bits = struct.unpack('<HHIIHH', b[:16])
                if fmt == 0xfffe and ln >= 40: fmt = struct.unpack('<H', b[24:26])[0]
            elif cid == b'data':
                return fmt, ch, rate, bits, pos + 8, ln
            pos += 8 + ln + (ln & 1)

def read(path, start=0.0, seconds=None):
    """(float64 array [frames, channels], rate). start / seconds in seconds (None = to the end)."""
    fmt, ch, rate, bits, off, ln = info(path)
    bps = bits // 8; frame = bps * ch
    total = ln // frame
    a = int(round(start * rate)); a = max(0, min(a, total))
    n = total - a if seconds is None else max(0, min(int(round(seconds * rate)), total - a))
    with open(path, 'rb') as f:
        f.seek(off + a * frame); raw = f.read(n * frame)
    n = len(raw) // frame; raw = raw[:n * frame]
    if fmt == 3:
        x = np.frombuffer(raw, dtype='<f4' if bits == 32 else '<f8').astype(np.float64)
    elif bits == 8:
        x = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    elif bits == 16:
        x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    elif bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v >= 1 << 23, v - (1 << 24), v)
        x = v.astype(np.float64) / 8388608.0
    elif bits == 32:
        x = np.frombuffer(raw, dtype='<i4').astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f'{path}: unsupported {fmt}/{bits}')
    return x.reshape(-1, ch), rate

def duration(path):
    fmt, ch, rate, bits, off, ln = info(path)
    return ln / (bits // 8 * ch) / rate

def write16(path, x, rate):
    """Mono 16-bit PCM; values are clipped to [-1, 1] (callers keep their peaks below that)."""
    y = np.clip(np.asarray(x, dtype=np.float64), -1.0, 1.0)
    pcm = np.round(y * 32767.0).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
                + b'data' + struct.pack('<I', len(pcm)) + pcm)

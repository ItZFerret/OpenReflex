// MX vs ATV Untamed converter, foundation: the client's game library as files, in pure C# (the client itself reads archives through the
// core's native library; the converter and its tests must not need it).
//   UxZstd   a zstd frame decoder (RFC 8878: raw / RLE / compressed blocks, Huffman literals, FSE sequences, repeat offsets; no
//            dictionaries). The content checksum is not checked: UxRxar checks each file's CRC-32 from the archive index instead.
//   UxRxar   a Reflex archive (.rxdata / .rxtrack version 5, container RXAR versions 1 and 2: core/plan/RXARCHIVE.md): header and index
//            validated, files stored / one zstd frame / chunked frames, pool references.
//   UxVfs    a library or install root: loose files first, then Tracks/<level>.rxtrack, then the root's *.rxdata (name order), pool
//            references resolved against Pool/<sha256> of the .rxdata files; the rule of Content/RxFiles.cs and core/src/fmt_vfs.c.
// Reads are thread safe.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxZstd
    {
        static readonly short[] LlDefault = { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1 };
        static readonly short[] MlDefault = { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1 };
        static readonly short[] OfDefault = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1 };
        static readonly int[] LlBase = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
        static readonly byte[] LlBits = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
        static readonly int[] MlBase = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99, 131, 259, 515, 1027, 2051, 4099, 8195, 16387, 32771, 65539 };
        static readonly byte[] MlBits = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

        static void Check(bool ok, string what) { if (!ok) throw new UxException("zstd: " + what); }
        static int HighBit(uint v) { int n = -1; while (v != 0) { n++; v >>= 1; } return n; }

        sealed class Fse
        {
            public int log; public byte[] symbol; public byte[] bits; public ushort[] baseState;
            public static Fse Build(short[] norm, int count, int log)
            {
                int size = 1 << log; var t = new Fse { log = log, symbol = new byte[size], bits = new byte[size], baseState = new ushort[size] };
                var next = new int[count]; int high = size;
                for (int s = 0; s < count; s++) if (norm[s] == -1) { t.symbol[--high] = (byte)s; next[s] = 1; }
                int step = (size >> 1) + (size >> 3) + 3, mask = size - 1, pos = 0;
                for (int s = 0; s < count; s++)
                {
                    if (norm[s] <= 0) continue;
                    next[s] = norm[s];
                    for (int i = 0; i < norm[s]; i++)
                    {
                        t.symbol[pos] = (byte)s;
                        do { pos = (pos + step) & mask; } while (pos >= high);
                    }
                }
                Check(pos == 0, "FSE table");
                for (int i = 0; i < size; i++)
                {
                    int sym = t.symbol[i], d = next[sym]++;
                    int nb = log - HighBit((uint)d);
                    t.bits[i] = (byte)nb; t.baseState[i] = (ushort)((d << nb) - size);
                }
                return t;
            }
            public static Fse Rle(byte sym) => new Fse { log = 0, symbol = new[] { sym }, bits = new byte[] { 0 }, baseState = new ushort[] { 0 } };

            /// An FSE table description (forward bits); returns the bytes used.
            public static Fse Read(byte[] src, int p, int end, int maxLog, int maxSymbols, out int used)
            {
                long bitPos = 0; long limit = (long)(end - p) * 8;
                uint Bits(int n)
                {
                    uint v = 0;
                    for (int i = 0; i < n; i++)
                    {
                        long at = bitPos + i;
                        Check(at < limit, "truncated FSE table");
                        v |= (uint)((src[p + (int)(at >> 3)] >> (int)(at & 7)) & 1) << i;
                    }
                    bitPos += n; return v;
                }
                int log = 5 + (int)Bits(4);
                Check(log <= maxLog, "FSE accuracy log");
                int remaining = 1 << log, symb = 0; var norm = new short[maxSymbols];
                while (remaining > 0 && symb < maxSymbols)
                {
                    int nb = HighBit((uint)(remaining + 1)) + 1;
                    int val = (int)Bits(nb);
                    int lower = (1 << (nb - 1)) - 1, threshold = (1 << nb) - 1 - (remaining + 1);
                    if ((val & lower) < threshold) { bitPos--; val &= lower; }
                    else if (val > lower) val -= threshold;
                    int proba = val - 1;
                    remaining -= proba < 0 ? -proba : proba;
                    norm[symb++] = (short)proba;
                    if (proba == 0)
                    {
                        int repeat = (int)Bits(2);
                        while (true)
                        {
                            for (int i = 0; i < repeat && symb < maxSymbols; i++) norm[symb++] = 0;
                            if (repeat == 3) repeat = (int)Bits(2); else break;
                        }
                    }
                }
                Check(remaining == 0 && symb <= maxSymbols, "FSE distribution");
                used = (int)((bitPos + 7) >> 3);
                return Build(norm, symb, log);
            }
        }

        /// A backward bit stream (the last byte holds the end marker).
        struct Back
        {
            byte[] b; int start; public long off;       // off: bits left (may go negative: reads past the start give zeros)
            public Back(byte[] b, int start, int length)
            {
                this.b = b; this.start = start;
                Check(length > 0 && b[start + length - 1] != 0, "bit stream end marker");
                off = (long)(length - 1) * 8 + HighBit(b[start + length - 1]);
            }
            public uint Read(int n)
            {
                if (n == 0) return 0;
                off -= n;
                return Peek(off, n);
            }
            /// n bits at bit offset o (bits below the start are zeros).
            public uint Peek(long o, int n)
            {
                int shift = 0;
                if (o < 0) { shift = (int)Math.Min(-o, 64); n -= shift; o = 0; if (n <= 0) return 0; }
                int at = start + (int)(o >> 3), bit = (int)(o & 7), need = bit + n;
                ulong v = 0; int have = 0, i = 0;
                while (have < need && at + i < b.Length) { v |= (ulong)b[at + i] << have; have += 8; i++; }
                return (uint)((v >> bit) & ((1UL << n) - 1)) << shift;
            }
        }

        sealed class State
        {
            public Fse ll, of, ml; public byte[] hufSymbol, hufBits; public int hufLog;
            public int rep0 = 1, rep1 = 4, rep2 = 8;
        }

        /// The uncompressed bytes of the zstd frames in src[start, start + length). expected: the size to check against (-1: unknown).
        public static byte[] Decompress(byte[] src, int start, int length, long expected = -1)
        {
            byte[] o = new byte[expected >= 0 ? expected : Math.Max(1024, (long)length * 4)]; int n = 0;
            int p = start, end = start + length;
            while (p < end)
            {
                Check(p + 4 <= end, "truncated frame");
                uint magic = UxBin.U32LE(src, p);
                if ((magic & 0xFFFFFFF0u) == 0x184D2A50u)
                {   // a skippable frame
                    Check(p + 8 <= end, "truncated skippable frame");
                    long skip = UxBin.U32LE(src, p + 4);
                    Check(p + 8 + skip <= end, "truncated skippable frame");
                    p += 8 + (int)skip; continue;
                }
                Check(magic == 0xFD2FB528u, "not a zstd frame");
                p += 4;
                Check(p < end, "truncated frame header");
                int fhd = src[p++];
                int fcsFlag = fhd >> 6; bool single = (fhd & 0x20) != 0, checksum = (fhd & 4) != 0; int dictFlag = fhd & 3;
                Check((fhd & 8) == 0, "reserved frame header bit");
                if (!single) { Check(p < end, "truncated frame header"); p++; }
                int dictBytes = dictFlag == 0 ? 0 : dictFlag == 1 ? 1 : dictFlag == 2 ? 2 : 4;
                Check(p + dictBytes <= end, "truncated frame header");
                for (int i = 0; i < dictBytes; i++) Check(src[p + i] == 0, "dictionaries are not supported");
                p += dictBytes;
                int fcsBytes = fcsFlag == 0 ? (single ? 1 : 0) : fcsFlag == 1 ? 2 : fcsFlag == 2 ? 4 : 8;
                Check(p + fcsBytes <= end, "truncated frame header");
                long content = -1;
                if (fcsBytes == 1) content = src[p];
                else if (fcsBytes == 2) content = UxBin.U16LE(src, p) + 256;
                else if (fcsBytes == 4) content = UxBin.U32LE(src, p);
                else if (fcsBytes == 8) content = (long)UxBin.U64(src, p, false);
                p += fcsBytes;
                int frameStart = n;
                if (content >= 0) { Check(content <= int.MaxValue - 64 - n, "frame content size"); if (n + content > o.Length) Array.Resize(ref o, (int)(n + content)); }
                var st = new State();
                while (true)
                {
                    Check(p + 3 <= end, "truncated block header");
                    int h = src[p] | src[p + 1] << 8 | src[p + 2] << 16; p += 3;
                    bool last = (h & 1) != 0; int type = (h >> 1) & 3, size = h >> 3;
                    if (type == 0)
                    {
                        Check(p + size <= end, "truncated raw block");
                        Grow(ref o, n + size); Buffer.BlockCopy(src, p, o, n, size); n += size; p += size;
                    }
                    else if (type == 1)
                    {
                        Check(p + 1 <= end, "truncated RLE block");
                        Grow(ref o, n + size); byte v = src[p++];
                        for (int i = 0; i < size; i++) o[n + i] = v;
                        n += size;
                    }
                    else
                    {
                        Check(type == 2, "reserved block type");
                        Check(p + size <= end, "truncated compressed block");
                        Block(src, p, p + size, ref o, ref n, st, frameStart);
                        p += size;
                    }
                    if (last) break;
                }
                if (content >= 0) Check(n - frameStart == content, "frame content size differs");
                if (checksum) { Check(p + 4 <= end, "truncated checksum"); p += 4; }
            }
            if (expected >= 0) Check(n == expected, "decoded " + n + " bytes, expected " + expected);
            if (n == o.Length) return o;
            var r = new byte[n]; Buffer.BlockCopy(o, 0, r, 0, n); return r;
        }

        static void Grow(ref byte[] o, long need)
        {
            if (need <= o.Length) return;
            Check(need <= int.MaxValue - 64, "output too large");
            long cap = o.Length; while (cap < need) cap = Math.Min(int.MaxValue - 64, Math.Max(cap * 2, 1024));
            Array.Resize(ref o, (int)cap);
        }

        static void Block(byte[] src, int p, int end, ref byte[] o, ref int n, State st, int frameStart)
        {
            // ---- literals
            Check(p < end, "empty block");
            int b0 = src[p], ltype = b0 & 3, sf = (b0 >> 2) & 3;
            byte[] lit; int litLen;
            if (ltype == 0 || ltype == 1)
            {
                int regen, hs;
                if ((sf & 1) == 0) { regen = b0 >> 3; hs = 1; }
                else if (sf == 1) { Check(p + 2 <= end, "literals header"); regen = (b0 >> 4) + (src[p + 1] << 4); hs = 2; }
                else { Check(p + 3 <= end, "literals header"); regen = (b0 >> 4) + (src[p + 1] << 4) + (src[p + 2] << 12); hs = 3; }
                p += hs;
                lit = new byte[regen]; litLen = regen;
                if (ltype == 0) { Check(p + regen <= end, "raw literals"); Buffer.BlockCopy(src, p, lit, 0, regen); p += regen; }
                else { Check(p + 1 <= end, "RLE literals"); byte v = src[p++]; for (int i = 0; i < regen; i++) lit[i] = v; }
            }
            else
            {
                int regen, comp, hs, streams;
                if (sf == 0 || sf == 1)
                {
                    Check(p + 3 <= end, "literals header");
                    int v = b0 | src[p + 1] << 8 | src[p + 2] << 16;
                    regen = (v >> 4) & 0x3FF; comp = (v >> 14) & 0x3FF; hs = 3; streams = sf == 0 ? 1 : 4;
                }
                else if (sf == 2)
                {
                    Check(p + 4 <= end, "literals header");
                    uint v = UxBin.U32LE(src, p);
                    regen = (int)((v >> 4) & 0x3FFF); comp = (int)((v >> 18) & 0x3FFF); hs = 4; streams = 4;
                }
                else
                {
                    Check(p + 5 <= end, "literals header");
                    ulong v = UxBin.U32LE(src, p) | (ulong)src[p + 4] << 32;
                    regen = (int)((v >> 4) & 0x3FFFF); comp = (int)((v >> 22) & 0x3FFFF); hs = 5; streams = 4;
                }
                p += hs;
                Check(p + comp <= end, "compressed literals");
                int lp = p, lend = p + comp; p = lend;
                if (ltype == 2) lp += ReadHuffman(src, lp, lend, st);
                else Check(st.hufSymbol != null, "treeless literals without a table");
                lit = new byte[regen]; litLen = regen;
                if (streams == 1) HufStream(src, lp, lend - lp, lit, 0, regen, st);
                else
                {
                    Check(lp + 6 <= lend, "literal streams jump table");
                    int s1 = UxBin.U16LE(src, lp), s2 = UxBin.U16LE(src, lp + 2), s3 = UxBin.U16LE(src, lp + 4);
                    lp += 6;
                    int s4 = lend - lp - s1 - s2 - s3;
                    Check(s4 >= 0, "literal stream sizes");
                    int per = (regen + 3) / 4;
                    Check(per * 3 <= regen, "literal stream lengths");
                    HufStream(src, lp, s1, lit, 0, per, st);
                    HufStream(src, lp + s1, s2, lit, per, per, st);
                    HufStream(src, lp + s1 + s2, s3, lit, 2 * per, per, st);
                    HufStream(src, lp + s1 + s2 + s3, s4, lit, 3 * per, regen - 3 * per, st);
                }
            }
            // ---- sequences
            if (p == end) { Grow(ref o, (long)n + litLen); Buffer.BlockCopy(lit, 0, o, n, litLen); n += litLen; return; }
            int nseq = src[p++];
            if (nseq >= 128)
            {
                if (nseq < 255) { Check(p < end, "sequence count"); nseq = ((nseq - 128) << 8) + src[p++]; }
                else { Check(p + 2 <= end, "sequence count"); nseq = src[p] + (src[p + 1] << 8) + 0x7F00; p += 2; }
            }
            if (nseq == 0) { Check(p == end, "bytes after an empty sequence section"); Grow(ref o, (long)n + litLen); Buffer.BlockCopy(lit, 0, o, n, litLen); n += litLen; return; }
            Check(p < end, "sequence modes");
            int modes = src[p++];
            Check((modes & 3) == 0, "reserved sequence mode bits");
            st.ll = Table(src, ref p, end, (modes >> 6) & 3, st.ll, LlDefault, 6, 9, 36);
            st.of = Table(src, ref p, end, (modes >> 4) & 3, st.of, OfDefault, 5, 8, 32);
            st.ml = Table(src, ref p, end, (modes >> 2) & 3, st.ml, MlDefault, 6, 9, 53);
            Check(p < end, "sequence bit stream");
            var bs = new Back(src, p, end - p);
            int sll = (int)bs.Read(st.ll.log), sof = (int)bs.Read(st.of.log), sml = (int)bs.Read(st.ml.log);
            int lpos = 0;
            for (int i = 0; i < nseq; i++)
            {
                int ofCode = st.of.symbol[sof], llCode = st.ll.symbol[sll], mlCode = st.ml.symbol[sml];
                Check(llCode < 36 && mlCode < 53 && ofCode < 32, "sequence code");
                long ofValue = (1L << ofCode) + bs.Read(ofCode);
                int mlen = MlBase[mlCode] + (int)bs.Read(MlBits[mlCode]);
                int llen = LlBase[llCode] + (int)bs.Read(LlBits[llCode]);
                if (i + 1 < nseq)
                {
                    sll = st.ll.baseState[sll] + (int)bs.Read(st.ll.bits[sll]);
                    sml = st.ml.baseState[sml] + (int)bs.Read(st.ml.bits[sml]);
                    sof = st.of.baseState[sof] + (int)bs.Read(st.of.bits[sof]);
                }
                Check(bs.off >= 0, "sequence bit stream overrun");
                long offset;
                if (ofValue > 3) { offset = ofValue - 3; st.rep2 = st.rep1; st.rep1 = st.rep0; st.rep0 = (int)offset; }
                else
                {
                    int idx = (int)ofValue - 1;
                    if (llen == 0) idx++;
                    if (idx == 0) offset = st.rep0;
                    else
                    {
                        offset = idx == 1 ? st.rep1 : idx == 2 ? st.rep2 : st.rep0 - 1;
                        if (idx > 1) st.rep2 = st.rep1;
                        st.rep1 = st.rep0; st.rep0 = (int)offset;
                    }
                }
                Check(lpos + llen <= litLen, "literal overrun");
                Grow(ref o, (long)n + llen + mlen);
                Buffer.BlockCopy(lit, lpos, o, n, llen); n += llen; lpos += llen;
                Check(offset > 0 && offset <= n - frameStart, "match offset outside the frame");
                int from = n - (int)offset;
                if (offset >= mlen) { Buffer.BlockCopy(o, from, o, n, mlen); n += mlen; }
                else for (int k = 0; k < mlen; k++) o[n++] = o[from + k];
            }
            Check(bs.off == 0, "sequence bit stream not consumed");
            int rest = litLen - lpos;
            Grow(ref o, (long)n + rest); Buffer.BlockCopy(lit, lpos, o, n, rest); n += rest;
        }

        static Fse Table(byte[] src, ref int p, int end, int mode, Fse previous, short[] def, int defLog, int maxLog, int maxSymbols)
        {
            switch (mode)
            {
                case 0: return Fse.Build(def, def.Length, defLog);
                case 1: Check(p < end, "RLE table"); return Fse.Rle(src[p++]);
                case 2: { var t = Fse.Read(src, p, end, maxLog, maxSymbols, out int used); p += used; return t; }
                default: Check(previous != null, "repeat table without a previous one"); return previous;
            }
        }

        static int ReadHuffman(byte[] src, int p, int end, State st)
        {
            Check(p < end, "Huffman table");
            int hb = src[p], used; var weights = new byte[256]; int count;
            if (hb >= 128)
            {
                count = hb - 127; int bytes = (count + 1) / 2;
                Check(p + 1 + bytes <= end, "Huffman weights");
                for (int i = 0; i < count; i++) { int v = src[p + 1 + i / 2]; weights[i] = (byte)((i & 1) == 0 ? v >> 4 : v & 15); }
                used = 1 + bytes;
            }
            else
            {
                Check(hb > 0 && p + 1 + hb <= end, "Huffman weights");
                var t = Fse.Read(src, p + 1, p + 1 + hb, 6, 256, out int tu);
                Check(tu < hb, "Huffman weight stream");
                var bs = new Back(src, p + 1 + tu, hb - tu);
                int s1 = (int)bs.Read(t.log), s2 = (int)bs.Read(t.log);
                count = 0;
                while (true)
                {
                    Check(count < 254, "Huffman weight count");
                    weights[count++] = t.symbol[s1];
                    s1 = t.baseState[s1] + (int)bs.Read(t.bits[s1]);
                    if (bs.off < 0) { weights[count++] = t.symbol[s2]; break; }
                    Check(count < 254, "Huffman weight count");
                    weights[count++] = t.symbol[s2];
                    s2 = t.baseState[s2] + (int)bs.Read(t.bits[s2]);
                    if (bs.off < 0) { weights[count++] = t.symbol[s1]; break; }
                }
                used = 1 + hb;
            }
            long sum = 0;
            for (int i = 0; i < count; i++) { Check(weights[i] <= 11, "Huffman weight"); if (weights[i] > 0) sum += 1L << (weights[i] - 1); }
            Check(sum > 0, "Huffman weights");
            int maxBits = HighBit((uint)sum) + 1;
            long left = (1L << maxBits) - sum;
            Check(left > 0 && (left & (left - 1)) == 0 && maxBits <= 11, "Huffman weights do not complete a tree");
            weights[count++] = (byte)(HighBit((uint)left) + 1);
            int size = 1 << maxBits;
            var sym = new byte[size]; var nb = new byte[size]; int pos = 0;
            for (int w = 1; w <= maxBits; w++)
                for (int s = 0; s < count; s++)
                {
                    if (weights[s] != w) continue;
                    int len = 1 << (w - 1);
                    for (int i = 0; i < len; i++) { sym[pos + i] = (byte)s; nb[pos + i] = (byte)(maxBits + 1 - w); }
                    pos += len;
                }
            Check(pos == size, "Huffman table");
            st.hufSymbol = sym; st.hufBits = nb; st.hufLog = maxBits;
            return used;
        }

        static void HufStream(byte[] src, int p, int length, byte[] lit, int at, int count, State st)
        {
            if (count == 0 && length == 0) return;
            var bs = new Back(src, p, length);
            int log = st.hufLog; byte[] sym = st.hufSymbol, nb = st.hufBits;
            for (int i = 0; i < count; i++)
            {
                int v = (int)bs.Peek(bs.off - log, log);
                lit[at + i] = sym[v];
                bs.off -= nb[v];
            }
            Check(bs.off == 0, "literal stream not consumed");
        }
    }

    /// A Reflex archive (RXAR).
    public sealed class UxRxar : IDisposable
    {
        public sealed class Entry
        {
            public string path; public int codec, chunkLog2; public long raw, offset, stored; public uint crc; public string sha256; public int[] chunks;
        }
        public readonly string file; public readonly uint kind, version; public readonly string meta;
        public readonly List<Entry> entries = new List<Entry>();
        readonly Dictionary<string, Entry> _by = new Dictionary<string, Entry>(StringComparer.Ordinal);
        readonly FileStream _f; readonly object _gate = new object();

        public static string Key(string path) => UxCommon.PyLower(path.Replace('\\', '/'));
        static void Check(bool ok, string file, string what) { if (!ok) throw new UxException(Path.GetFileName(file) + ": " + what); }

        public UxRxar(string file)
        {
            this.file = file;
            _f = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.Read);
            try
            {
                long size = _f.Length;
                Check(size >= 64, file, "not a Reflex archive");
                var h = ReadAt(0, 64);
                Check(h[0] == 'R' && h[1] == 'X' && h[2] == 'A' && h[3] == 'R', file, "not a Reflex archive (unfinished?)");
                version = UxBin.U32LE(h, 4); kind = UxBin.U32LE(h, 8);
                Check(version == 1 || version == 2, file, "archive container version " + version);
                Check(UxBin.U32LE(h, 12) == 0, file, "archive flags");
                Check(UxBin.Crc32(h, 0, 60) == UxBin.U32LE(h, 60), file, "archive header checksum");
                long indexOffset = (long)UxBin.U64(h, 16, false), indexStored = (long)UxBin.U64(h, 24, false), indexRaw = (long)UxBin.U64(h, 32, false);
                long count = UxBin.U32LE(h, 40);
                Check(indexOffset >= 64 && indexStored > 0 && indexOffset + indexStored <= size && indexRaw >= 12 && indexRaw <= 1 << 30 && indexStored <= 1 << 30, file, "archive index range");
                var ix = UxZstd.Decompress(ReadAt(indexOffset, (int)indexStored), 0, (int)indexStored, indexRaw);
                Check(UxBin.Crc32(ix, 0, ix.Length) == UxBin.U32LE(h, 44), file, "archive index checksum");
                Check(UxBin.U32LE(ix, 0) == 0x58495852 && UxBin.U32LE(ix, 4) == count, file, "archive index header");   // "RXIX"
                int m = (int)UxBin.U32LE(ix, 8), p = 12;
                Check(m >= 0 && p + m <= ix.Length, file, "archive metadata");
                meta = Encoding.UTF8.GetString(ix, p, m); p += m;
                for (long i = 0; i < count; i++)
                {
                    Check(p + 2 <= ix.Length, file, "archive index entry");
                    int pl = UxBin.U16LE(ix, p); p += 2;
                    Check(pl > 0 && p + pl + 32 <= ix.Length, file, "archive index entry");
                    var e = new Entry { path = Encoding.UTF8.GetString(ix, p, pl) }; p += pl;
                    e.codec = ix[p]; e.chunkLog2 = ix[p + 1]; int flags = UxBin.U16LE(ix, p + 2);
                    e.raw = (long)UxBin.U64(ix, p + 4, false); e.offset = (long)UxBin.U64(ix, p + 12, false); e.stored = (long)UxBin.U64(ix, p + 20, false);
                    e.crc = UxBin.U32LE(ix, p + 28); p += 32;
                    Check(e.codec <= (version == 2 ? 2 : 1) && (flags & ~1) == 0 && (flags == 0 || version == 2), file, "archive entry codec / flags");
                    if ((flags & 1) != 0) { Check(p + 32 <= ix.Length, file, "archive index entry"); e.sha256 = UxBin.Hex(UxBin.Slice(ix, p, 32)); p += 32; }
                    if (e.chunkLog2 != 0)
                    {
                        Check(e.codec == 1 && e.chunkLog2 >= 12 && e.chunkLog2 <= 30 && p + 4 <= ix.Length, file, "archive chunked entry");
                        long nc = UxBin.U32LE(ix, p); p += 4;
                        Check(nc == (e.raw + (1L << e.chunkLog2) - 1) >> e.chunkLog2 && p + 4 * nc <= ix.Length, file, "archive chunk count");
                        e.chunks = new int[nc]; long sum = 0;
                        for (int k = 0; k < nc; k++) { e.chunks[k] = (int)UxBin.U32LE(ix, p + 4 * k); sum += e.chunks[k]; }
                        Check(sum == e.stored, file, "archive chunk sizes");
                        p += 4 * (int)nc;
                    }
                    if (e.codec == 2) Check(e.offset == 0 && e.stored == 0 && e.sha256 != null, file, "archive pool reference");
                    else Check(e.offset >= 64 && e.offset + e.stored <= indexOffset && (e.codec != 0 || e.stored == e.raw), file, "archive file range");
                    Check(e.raw <= int.MaxValue - 64, file, "archive file size");
                    string key = Key(e.path);
                    Check(!_by.ContainsKey(key), file, "archive path twice: " + e.path);
                    _by[key] = e; entries.Add(e);
                }
                Check(p == ix.Length, file, "archive index length");
            }
            catch { _f.Dispose(); throw; }
        }

        byte[] ReadAt(long offset, int size)
        {
            var b = new byte[size];
            lock (_gate)
            {
                _f.Position = offset;
                int got = 0; while (got < size) { int n = _f.Read(b, got, size - got); if (n <= 0) break; got += n; }
                if (got != size) throw new UxException(Path.GetFileName(file) + ": read past the end of the archive");
            }
            return b;
        }

        public Entry Find(string path) => _by.TryGetValue(Key(path), out var e) ? e : null;

        /// The file's bytes (CRC-32 checked). A pool reference has no bytes here (UxVfs resolves it).
        public byte[] Read(Entry e)
        {
            Check(e.codec != 2, file, e.path + " is a pool reference");
            byte[] r;
            if (e.codec == 0) r = ReadAt(e.offset, (int)e.raw);
            else if (e.chunkLog2 == 0) r = UxZstd.Decompress(ReadAt(e.offset, (int)e.stored), 0, (int)e.stored, e.raw);
            else
            {
                r = new byte[e.raw]; long at = e.offset, done = 0, chunk = 1L << e.chunkLog2;
                foreach (int c in e.chunks)
                {
                    long want = Math.Min(chunk, e.raw - done);
                    var part = UxZstd.Decompress(ReadAt(at, c), 0, c, want);
                    Buffer.BlockCopy(part, 0, r, (int)done, (int)want);
                    at += c; done += want;
                }
            }
            Check(UxBin.Crc32(r, 0, r.Length) == e.crc, file, e.path + ": checksum differs (the archive is damaged)");
            return r;
        }

        /// count bytes at offset of the file (chunked files decode only the chunks the range covers; other files are read whole).
        public byte[] ReadRange(Entry e, long offset, int count)
        {
            Check(e.codec != 2, file, e.path + " is a pool reference");
            Check(offset >= 0 && count >= 0 && offset + count <= e.raw, file, e.path + ": range outside the file");
            if (e.codec == 0) return ReadAt(e.offset + offset, count);
            if (e.chunkLog2 == 0) return UxBin.Slice(Read(e), (int)offset, count);
            var r = new byte[count]; long chunk = 1L << e.chunkLog2, at = e.offset;
            int first = (int)(offset >> e.chunkLog2), last = count == 0 ? first - 1 : (int)((offset + count - 1) >> e.chunkLog2);
            for (int k = 0; k < e.chunks.Length && k <= last; k++)
            {
                if (k >= first)
                {
                    long cs = k * chunk, want = Math.Min(chunk, e.raw - cs);
                    var part = UxZstd.Decompress(ReadAt(at, e.chunks[k]), 0, e.chunks[k], want);
                    long from = Math.Max(offset, cs), to = Math.Min(offset + count, cs + want);
                    Buffer.BlockCopy(part, (int)(from - cs), r, (int)(from - offset), (int)(to - from));
                }
                at += e.chunks[k];
            }
            return r;
        }

        public void Dispose() { _f.Dispose(); }
    }

    /// A library / install root as the game reads it: loose files, then Tracks/*.rxtrack, then the root's *.rxdata.
    public sealed class UxVfs : IDisposable
    {
        public readonly string root;
        readonly object _gate = new object();
        List<UxRxar> _archives; List<UxRxar> _data;
        public readonly List<string> skipped = new List<string>();

        public UxVfs(string root) { this.root = Path.GetFullPath(root); }

        string Loose(string relative) => Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar));

        void Open()
        {
            lock (_gate)
            {
                if (_archives != null) return;
                var all = new List<UxRxar>(); var data = new List<UxRxar>();
                void Add(string f, bool isData)
                {
                    try
                    {
                        using (var s = File.OpenRead(f))
                        {
                            var m = new byte[4];
                            if (s.Read(m, 0, 4) != 4 || m[0] != 'R' || m[1] != 'X' || m[2] != 'A' || m[3] != 'R') return;   // a legacy gzip .rxtrack: not an overlay
                        }
                        var a = new UxRxar(f); all.Add(a); if (isData) data.Add(a);
                    }
                    catch (Exception e) when (e is UxException || e is IOException || e is UnauthorizedAccessException) { skipped.Add(Path.GetFileName(f) + ": " + e.Message); }
                }
                string tracks = Path.Combine(root, "Tracks");
                if (Directory.Exists(tracks))
                {
                    var files = Directory.GetFiles(tracks, "*.rxtrack"); Array.Sort(files, StringComparer.OrdinalIgnoreCase);
                    foreach (var f in files) Add(f, false);
                }
                if (Directory.Exists(root))
                {
                    var files = Directory.GetFiles(root, "*.rxdata"); Array.Sort(files, StringComparer.OrdinalIgnoreCase);
                    foreach (var f in files) Add(f, true);
                }
                _data = data; _archives = all;
            }
        }
        public int ArchiveCount { get { Open(); return _archives.Count; } }

        (UxRxar a, UxRxar.Entry e) InArchive(string relative)
        {
            Open();
            foreach (var a in _archives) { var e = a.Find(relative); if (e != null) return (a, e); }
            return (null, null);
        }
        (UxRxar a, UxRxar.Entry e) Pool(UxRxar.Entry reference, string relative)
        {
            foreach (var a in _data)
            {
                var e = a.Find("Pool/" + reference.sha256);
                if (e != null) { UxBin.Require(e.raw == reference.raw && e.codec != 2, relative + ": pool file size differs"); return (a, e); }
            }
            throw new UxException(relative + ": its pool file Pool/" + reference.sha256 + " is missing");
        }

        public bool Exists(string relative) => File.Exists(Loose(relative)) || InArchive(relative).e != null;
        /// The file is not loose: it comes from an archive of the root.
        public bool Packed(string relative) => !File.Exists(Loose(relative)) && InArchive(relative).e != null;
        public long Length(string relative)
        {
            string l = Loose(relative);
            if (File.Exists(l)) return new FileInfo(l).Length;
            var (_, e) = InArchive(relative);
            return e == null ? -1 : e.raw;
        }
        public byte[] ReadAllBytes(string relative)
        {
            string l = Loose(relative);
            if (File.Exists(l)) return File.ReadAllBytes(l);
            var (a, e) = InArchive(relative);
            if (e == null) throw new UxException("missing " + relative + " in " + root);
            if (e.codec == 2) (a, e) = Pool(e, relative);
            return a.Read(e);
        }
        public byte[] ReadRange(string relative, long offset, int count)
        {
            string l = Loose(relative);
            if (File.Exists(l))
            {
                using (var s = new FileStream(l, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    UxBin.Require(offset >= 0 && offset + count <= s.Length, relative + ": range outside the file");
                    var b = new byte[count]; s.Position = offset;
                    int got = 0; while (got < count) { int n = s.Read(b, got, count - got); if (n <= 0) break; got += n; }
                    UxBin.Require(got == count, relative + ": truncated");
                    return b;
                }
            }
            var (a, e) = InArchive(relative);
            if (e == null) throw new UxException("missing " + relative + " in " + root);
            if (e.codec == 2) (a, e) = Pool(e, relative);
            return a.ReadRange(e, offset, count);
        }
        /// The file names directly in a folder of the root (loose and packed), each once.
        public List<string> ListDir(string relativeDir)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase); var r = new List<string>();
            string l = Loose(relativeDir);
            if (Directory.Exists(l)) foreach (var f in Directory.GetFiles(l)) if (seen.Add(Path.GetFileName(f))) r.Add(Path.GetFileName(f));
            Open();
            string prefix = UxRxar.Key(relativeDir.TrimEnd('/') + "/");
            foreach (var a in _archives)
                foreach (var e in a.entries)
                {
                    string k = UxRxar.Key(e.path);
                    if (k.StartsWith(prefix, StringComparison.Ordinal) && k.IndexOf('/', prefix.Length) < 0 && seen.Add(e.path.Substring(prefix.Length))) r.Add(e.path.Substring(prefix.Length));
                }
            r.Sort(StringComparer.OrdinalIgnoreCase);
            return r;
        }
        public void Dispose() { lock (_gate) { if (_archives != null) foreach (var a in _archives) a.Dispose(); _archives = null; _data = null; } }
    }
}

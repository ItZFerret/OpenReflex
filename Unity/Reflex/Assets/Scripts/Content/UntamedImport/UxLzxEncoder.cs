// MX vs ATV Untamed converter: the stored form of a package resource (the XMem chunk container of a Reflex PC .dx9.package) and a
// small LZX compressor for it. The reference compresses with Microsoft's XMemCompress, which the client cannot ship; this encoder
// writes the same container with its own LZX streams, which the game's own format readers decode:
//   container   u32 end, per 64 KiB chunk { u32 65536, u32 packed, packed bytes }, u32 the resource's whole size
//   chunk       one LZX stream (window 2^17, reset per chunk) cut into frames of 32768 output bytes; every frame but the last is
//               { u16be packed, bytes }, the last { 0xFF, u16be output, u16be packed, bytes }, then five zero bytes (as XMemCompress)
//   LZX         the E8 flag bit 0, then one VERBATIM block per frame (3 bits type 1, 24 bits size, the main tree 0..255 and 256..527
//               and the length tree as pre-tree coded deltas against the previous block's lengths), matches of 2..257 bytes with the
//               three repeated offsets, the frame padded to a 16-bit word
//   a chunk that does not shrink is STORED ({ u32 n, u32 n, n bytes }; n = 65536 or the last chunk's length): core fmt_lzx.c "a stored
//   chunk", client RfxData.Lzx the same
// Readers checked by tests/untamed_convert/build: UxLzx / LzxDecoder (the client's decoder), UxPkg.RfxLzx (the client's resource
// reader), core/src/fmt_lzx.c (the C core, also inside rxcore.dll in the headless race) and the reference's Microsoft codec helper.
// The match finder is a hash chain with one-step lazy matching; the output depends on the input bytes only (no threads, no clock).
// System only, C# 9, .NET Standard 2.1.
using System;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxLzxEncoder
    {
        public const int CHUNK = 0x10000, FRAME = 32768;
        const int MIN_MATCH = 2, MAX_MATCH = 257, NUM_CHARS = 256, SLOTS = 34, MAIN = NUM_CHARS + SLOTS * 8, LENGTHS = 249, PRETREE = 20;
        const int MAX_CHAIN = 48, LAZY_BELOW = 48, HASH_BITS = 16;
        static readonly int[] s_extra = new int[SLOTS + 2], s_base = new int[SLOTS + 2];
        static UxLzxEncoder()
        {
            for (int i = 0, j = 0; i < SLOTS + 2; i += 2) { s_extra[i] = s_extra[i + 1] = j; if (i != 0 && j < 17) j++; }
            for (int i = 0, j = 0; i < SLOTS + 2; i++) { s_base[i] = j; j += 1 << s_extra[i]; }
        }

        /// A resource as STORED chunks without padding: u32 container end, per chunk {u32 size, u32 size, the bytes} (size = 65536, the
        /// last chunk its own length), u32 the whole size.
        public static byte[] StoredBlob(byte[] data)
        {
            int n = data.Length, chunks = (n + CHUNK - 1) / CHUNK;
            var o = new byte[4 + 8L * chunks + n + 4];
            int p = 4;
            for (int at = 0; at < n; at += CHUNK)
            {
                int len = Math.Min(CHUNK, n - at);
                UxBin.PutU32(o, p, (uint)len); UxBin.PutU32(o, p + 4, (uint)len); p += 8;
                Buffer.BlockCopy(data, at, o, p, len); p += len;
            }
            UxBin.PutU32(o, 0, (uint)p);
            UxBin.PutU32(o, p, (uint)n);
            return o;
        }

        [ThreadStatic] static Encoder t_encoder;

        /// A resource as LZX chunks (a chunk that does not shrink is stored).
        public static byte[] EncodeBlob(byte[] data)
        {
            var enc = t_encoder ?? (t_encoder = new Encoder());
            int n = data.Length;
            var w = new UxWriter(n / 2 + 64);
            w.U32(0);
            for (int at = 0; at < n; at += CHUNK)
            {
                int len = Math.Min(CHUNK, n - at);
                int z = enc.Chunk(data, at, len);
                if (z <= 0 || z >= len) { w.U32((uint)len).U32((uint)len).Bytes(data, at, len); continue; }
                w.U32(CHUNK).U32((uint)z).Bytes(enc.output, 0, z);
            }
            w.PatchU32(0, (uint)w.Length);
            w.U32((uint)n);
            return w.ToArray();
        }

        sealed class Encoder
        {
            public readonly byte[] output = new byte[2 * CHUNK + 8192];
            readonly int[] head = new int[1 << HASH_BITS], prev = new int[CHUNK];
            // the tokens of a frame: main symbol, length symbol (-1: none), extra bits and their value
            readonly ushort[] tMain = new ushort[FRAME]; readonly short[] tLen = new short[FRAME]; readonly byte[] tBits = new byte[FRAME]; readonly int[] tVal = new int[FRAME];
            readonly int[] mainFreq = new int[MAIN], lenFreq = new int[LENGTHS], preFreq = new int[PRETREE];
            readonly byte[] mainPrev = new byte[MAIN], lenPrev = new byte[LENGTHS], mainLen = new byte[MAIN], lenLen = new byte[LENGTHS], preLen = new byte[PRETREE];
            readonly ushort[] mainCode = new ushort[MAIN], lenCode = new ushort[LENGTHS], preCode = new ushort[PRETREE];
            readonly byte[] frame = new byte[3 * FRAME];
            // the pre-tree symbols of one length table
            readonly byte[] pSym = new byte[MAIN], pBits = new byte[MAIN], pVal = new byte[MAIN];
            // Huffman work
            readonly int[] hSym = new int[MAIN], hWeight = new int[2 * MAIN], hParent = new int[2 * MAIN], hDepth = new int[2 * MAIN], blCount = new int[18], nextCode = new int[18];
            // bit writer
            ulong acc; int nbits, fn;
            int R0, R1, R2;
            byte[] d; int ds, dn;

            void Put(uint v, int count)
            {
                if (count == 0) return;
                acc = (acc << count) | v; nbits += count;
                while (nbits >= 16)
                {
                    nbits -= 16;
                    uint w = (uint)(acc >> nbits) & 0xFFFF;
                    frame[fn++] = (byte)w; frame[fn++] = (byte)(w >> 8);
                }
                acc &= (1UL << nbits) - 1;
            }

            /// Code lengths (at most maxBits, a complete code; at least two symbols when any is used) and the canonical codes.
            void Huffman(int[] freq, int n, int maxBits, byte[] len, ushort[] code)
            {
                int m = 0;
                for (int s = 0; s < n; s++) { len[s] = 0; if (freq[s] > 0) hSym[m++] = s; }
                if (m == 0) return;
                if (m == 1)
                {   // a one-symbol code is not complete: a second symbol that is never written
                    int other = hSym[0] == 0 ? 1 : 0;
                    len[hSym[0]] = 1; len[other] = 1;
                }
                else
                {
                    // leaves by (frequency, symbol): an insertion sort is enough for the small tables, a real sort for the main tree
                    Array.Sort(hSym, 0, m, new ByFreq(freq));
                    for (int i = 0; i < m; i++) hWeight[i] = freq[hSym[i]];
                    int leaf = 0, node = m, next = m;                // two queues: leaves [leaf, m), internal nodes [node, next)
                    while (m - leaf + next - node > 1)
                    {
                        int a = leaf < m && (node >= next || hWeight[leaf] <= hWeight[node]) ? leaf++ : node++;
                        int b = leaf < m && (node >= next || hWeight[leaf] <= hWeight[node]) ? leaf++ : node++;
                        hWeight[next] = hWeight[a] + hWeight[b]; hParent[a] = next; hParent[b] = next; next++;
                    }
                    int root = next - 1;
                    hDepth[root] = 0;
                    for (int i = root - 1; i >= 0; i--) hDepth[i] = hDepth[hParent[i]] + 1;
                    Array.Clear(blCount, 0, blCount.Length);
                    int overflow = 0;
                    for (int i = 0; i < m; i++) { int b = hDepth[i]; if (b > maxBits) { b = maxBits; overflow++; } blCount[b]++; }
                    while (overflow > 0)
                    {   // zlib's gen_bitlen: move a leaf down from the deepest level that has one, pair it with an overflowed leaf
                        int bits = maxBits - 1;
                        while (blCount[bits] == 0) bits--;
                        blCount[bits]--; blCount[bits + 1] += 2; blCount[maxBits]--;
                        overflow -= 2;
                    }
                    int at = 0;                                      // the least frequent leaves get the longest codes
                    for (int bits = maxBits; bits >= 1; bits--)
                        for (int k = blCount[bits]; k > 0; k--) len[hSym[at++]] = (byte)bits;
                }
                Array.Clear(blCount, 0, blCount.Length);
                for (int s = 0; s < n; s++) blCount[len[s]]++;
                blCount[0] = 0;
                int c = 0;
                for (int bits = 1; bits <= 16; bits++) { c = (c + blCount[bits - 1]) << 1; nextCode[bits] = c; }
                for (int s = 0; s < n; s++) if (len[s] != 0) code[s] = (ushort)nextCode[len[s]]++;
            }
            sealed class ByFreq : System.Collections.Generic.IComparer<int>
            {
                readonly int[] f; public ByFreq(int[] f) { this.f = f; }
                public int Compare(int a, int b) => f[a] != f[b] ? f[a].CompareTo(f[b]) : a.CompareTo(b);
            }

            /// The lengths cur[first..last) as pre-tree coded deltas against prev (which then holds them).
            void WriteLengths(byte[] prevLens, byte[] cur, int first, int last)
            {
                int n = 0;
                Array.Clear(preFreq, 0, PRETREE);
                void Sym(int s, int bits, int val) { pSym[n] = (byte)s; pBits[n] = (byte)bits; pVal[n] = (byte)val; n++; preFreq[s]++; }
                for (int i = first; i < last;)
                {
                    if (cur[i] == 0)
                    {
                        int r = 1; while (i + r < last && cur[i + r] == 0) r++;
                        while (r >= 20) { int t = Math.Min(r, 51); Sym(18, 5, t - 20); r -= t; i += t; }
                        if (r >= 4) { Sym(17, 4, r - 4); i += r; r = 0; }
                        for (; r > 0; r--, i++) Sym((prevLens[i] + 17) % 17, 0, 0);
                    }
                    else { Sym((prevLens[i] - cur[i] + 17) % 17, 0, 0); i++; }
                }
                Huffman(preFreq, PRETREE, 15, preLen, preCode);
                for (int s = 0; s < PRETREE; s++) Put(preLen[s], 4);
                for (int k = 0; k < n; k++) { Put(preCode[pSym[k]], preLen[pSym[k]]); Put(pVal[k], pBits[k]); }
                for (int i = first; i < last; i++) prevLens[i] = cur[i];
            }

            static int Hash(byte[] d, int p) => ((d[p] << 8 ^ d[p + 1] << 4 ^ d[p + 2]) * 0x9E3B >> 4) & ((1 << HASH_BITS) - 1);
            void Insert(int rel)
            {
                if (rel + 2 >= dn) return;
                int h = Hash(d, ds + rel);
                prev[rel] = head[h]; head[h] = rel;
            }
            /// The longest match (>= 3) at rel among the chain's candidates (the nearest of the longest).
            void Find(int rel, int maxLen, out int bestLen, out int bestOff)
            {
                bestLen = 0; bestOff = 0;
                if (maxLen < 3 || rel + 2 >= dn) return;
                int cand = head[Hash(d, ds + rel)], depth = MAX_CHAIN, p = ds + rel, best = 2;
                while (cand >= 0 && depth-- > 0)
                {
                    int q = ds + cand;
                    if (d[q + best] == d[p + best] && d[q] == d[p] && d[q + 1] == d[p + 1])
                    {
                        int l = 2; while (l < maxLen && d[q + l] == d[p + l]) l++;
                        if (l > best) { best = l; bestOff = rel - cand; if (l >= maxLen) break; }
                    }
                    cand = prev[cand];
                }
                if (best >= 3) bestLen = best;
            }
            /// The longest match (>= 2) at rel at one of the three repeated offsets (R0 first among equals).
            void Rep(int rel, int maxLen, out int bestLen, out int bestOff)
            {
                bestLen = 0; bestOff = 0;
                if (maxLen < 2) return;
                int p = ds + rel;
                for (int k = 0; k < 3; k++)
                {
                    int off = k == 0 ? R0 : k == 1 ? R1 : R2;
                    if (off > rel || k == 1 && off == R0 || k == 2 && (off == R0 || off == R1)) continue;
                    int q = p - off, l = 0;
                    while (l < maxLen && d[q + l] == d[p + l]) l++;
                    if (l >= 2 && l > bestLen) { bestLen = l; bestOff = off; }
                }
            }

            void Literal(ref int nt, int rel)
            {
                byte b = d[ds + rel];
                tMain[nt] = b; tLen[nt] = -1; tBits[nt] = 0; nt++; mainFreq[b]++;
            }
            void Match(ref int nt, int len, int off)
            {
                int slot, bits = 0, val = 0;
                if (off == R0) slot = 0;
                else if (off == R1) { slot = 1; R1 = R0; R0 = off; }
                else if (off == R2) { slot = 2; R2 = R0; R0 = off; }
                else
                {
                    int f = off + 2;
                    slot = 3; while (s_base[slot + 1] <= f) slot++;
                    bits = s_extra[slot]; val = f - s_base[slot];
                    R2 = R1; R1 = R0; R0 = off;
                }
                int lh = Math.Min(len - MIN_MATCH, 7);
                int main = NUM_CHARS + (slot << 3) + lh;
                tMain[nt] = (ushort)main; tBits[nt] = (byte)bits; tVal[nt] = val; mainFreq[main]++;
                if (lh == 7) { int ls = len - MIN_MATCH - 7; tLen[nt] = (short)ls; lenFreq[ls]++; } else tLen[nt] = -1;
                nt++;
            }

            /// One frame [fs, fe) of the chunk as a verbatim block into `frame`; returns its byte count.
            int Frame(int fs, int fe, bool first)
            {
                Array.Clear(mainFreq, 0, MAIN); Array.Clear(lenFreq, 0, LENGTHS);
                int nt = 0, pos = fs;
                while (pos < fe)
                {
                    int maxLen = Math.Min(MAX_MATCH, fe - pos);
                    Rep(pos, maxLen, out int repLen, out int repOff);
                    Find(pos, maxLen, out int bl, out int bo);
                    Insert(pos);
                    int len = 0, off = 0;
                    if (repLen >= 2 && repLen + 1 >= bl) { len = repLen; off = repOff; }
                    else if (bl >= 4 || bl == 3 && bo <= 4096) { len = bl; off = bo; }
                    else if (repLen >= 2) { len = repLen; off = repOff; }
                    if (len == 0) { Literal(ref nt, pos); pos++; continue; }
                    if (len < LAZY_BELOW && pos + 1 < fe)
                    {   // one-step lazy matching: a clearly longer match at the next byte wins
                        Find(pos + 1, Math.Min(MAX_MATCH, fe - pos - 1), out int nl, out int _);
                        if (nl > len + 1 || nl > len && off != repOff) { Literal(ref nt, pos); pos++; continue; }
                    }
                    Match(ref nt, len, off);
                    for (int k = 1; k < len; k++) Insert(pos + k);
                    pos += len;
                }
                // ---- the block
                Huffman(mainFreq, MAIN, 16, mainLen, mainCode);
                Huffman(lenFreq, LENGTHS, 16, lenLen, lenCode);
                fn = 0; acc = 0; nbits = 0;
                if (first) Put(0, 1);                               // no E8 (x86 call) translation
                int size = fe - fs;
                Put(1, 3); Put((uint)(size >> 8), 16); Put((uint)(size & 0xFF), 8);
                WriteLengths(mainPrev, mainLen, 0, NUM_CHARS);
                WriteLengths(mainPrev, mainLen, NUM_CHARS, MAIN);
                WriteLengths(lenPrev, lenLen, 0, LENGTHS);
                for (int k = 0; k < nt; k++)
                {
                    int m = tMain[k];
                    Put(mainCode[m], mainLen[m]);
                    if (m < NUM_CHARS) continue;
                    if (tLen[k] >= 0) Put(lenCode[tLen[k]], lenLen[tLen[k]]);
                    if (tBits[k] > 0) Put((uint)tVal[k], tBits[k]);
                }
                if (nbits > 0) Put(0, 16 - nbits);
                return fn;
            }

            /// The chunk data[start .. start + len) as frames into `output`; returns the byte count, or 0 when it cannot be framed.
            public int Chunk(byte[] data, int start, int len)
            {
                d = data; ds = start; dn = len;
                for (int i = 0; i < head.Length; i++) head[i] = -1;
                Array.Clear(mainPrev, 0, MAIN); Array.Clear(lenPrev, 0, LENGTHS);
                R0 = R1 = R2 = 1;
                int o = 0;
                for (int fs = 0; fs < len; fs += FRAME)
                {
                    int fe = Math.Min(len, fs + FRAME);
                    int n = Frame(fs, fe, fs == 0);
                    if (n >= 0xFF00 || o + n + 16 > output.Length) return 0;
                    if (fe == len) { output[o++] = 0xFF; output[o++] = (byte)((fe - fs) >> 8); output[o++] = (byte)(fe - fs); }
                    output[o++] = (byte)(n >> 8); output[o++] = (byte)n;
                    Buffer.BlockCopy(frame, 0, output, o, n); o += n;
                }
                for (int i = 0; i < 5; i++) output[o++] = 0;       // XMemCompress ends a chunk with five zero bytes
                return o;
            }
        }
    }
}

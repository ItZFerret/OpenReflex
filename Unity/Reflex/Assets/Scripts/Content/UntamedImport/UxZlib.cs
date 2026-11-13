// MX vs ATV Untamed converter, foundation: zlib as the Python reference uses it.
//   Compress    zlib.compress(data) of zlib 1.3.1 at its default level 6, byte for byte: a port of deflate.c (deflate_slow, longest_match,
//               fill_window) and trees.c (dynamic / static / stored block choice, Huffman construction). bxml._pack compresses every
//               version-0x10000 BXML document this way, so scene chunks, databases, level files and the library prototypes come out with
//               the bytes Python writes. The output depends only on the input (zlib's one-shot Z_FINISH call).
//   Decompress  zlib.decompress (System.IO.Compression inflate, Adler-32 checked here).
//   Stored      a zlib stream of stored blocks (the Alive importer's writer), for callers that prefer speed over parity.
using System;
using System.IO;
using System.IO.Compression;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxZlib
    {
        public static uint Adler32(byte[] data, int start, int length)
        {
            uint a = 1, s = 0;
            for (int i = 0; i < length;)
            {
                int n = Math.Min(3800, length - i);
                for (int k = 0; k < n; k++, i++) { a += data[start + i]; s += a; }
                a %= 65521; s %= 65521;
            }
            return (s << 16) | a;
        }

        /// zlib.decompress(data[start:start+length]).
        public static byte[] Decompress(byte[] data, int start, int length)
        {
            UxBin.Require(start >= 0 && length >= 6 && start + length <= data.Length, "zlib: truncated stream");
            UxBin.Require((data[start] & 0x0F) == 8 && ((data[start] << 8) | data[start + 1]) % 31 == 0 && (data[start + 1] & 0x20) == 0, "zlib: bad header");
            byte[] result;
            try
            {
                using (var input = new MemoryStream(data, start + 2, length - 2, false))
                using (var z = new DeflateStream(input, CompressionMode.Decompress))
                using (var o = new MemoryStream(Math.Max(256, length * 3)))
                {
                    z.CopyTo(o);
                    result = o.ToArray();
                }
            }
            catch (InvalidDataException e) { throw new UxException("zlib: " + e.Message, e); }
            return result;
        }
        public static byte[] Decompress(byte[] data) => Decompress(data, 0, data.Length);

        /// A zlib stream of stored deflate blocks (not what Python writes; decodes to the same bytes).
        public static byte[] Stored(byte[] payload)
        {
            var o = new MemoryStream(payload.Length + payload.Length / 65535 * 5 + 16);
            o.WriteByte(0x78); o.WriteByte(0x01);
            if (payload.Length == 0) { o.WriteByte(1); o.WriteByte(0); o.WriteByte(0); o.WriteByte(0xFF); o.WriteByte(0xFF); }
            for (int at = 0; at < payload.Length; at += 65535)
            {
                int n = Math.Min(65535, payload.Length - at);
                o.WriteByte((byte)(at + 65535 >= payload.Length ? 1 : 0));
                o.WriteByte((byte)n); o.WriteByte((byte)(n >> 8));
                int c = n ^ 0xFFFF; o.WriteByte((byte)c); o.WriteByte((byte)(c >> 8));
                o.Write(payload, at, n);
            }
            uint ad = Adler32(payload, 0, payload.Length);
            o.WriteByte((byte)(ad >> 24)); o.WriteByte((byte)(ad >> 16)); o.WriteByte((byte)(ad >> 8)); o.WriteByte((byte)ad);
            return o.ToArray();
        }

        /// zlib.compress(data): level 6, window 15, memLevel 8, default strategy; the bytes zlib 1.3.1 writes.
        public static byte[] Compress(byte[] data) => new Deflater(data).Run();

        sealed class Deflater
        {
            const int W_SIZE = 32768, W_MASK = W_SIZE - 1, HASH_SIZE = 32768, HASH_MASK = HASH_SIZE - 1, HASH_SHIFT = 5;
            const int MIN_MATCH = 3, MAX_MATCH = 258, MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1, MAX_DIST = W_SIZE - MIN_LOOKAHEAD;
            const int WINDOW_SIZE = 2 * W_SIZE, TOO_FAR = 4096, WIN_INIT = MAX_MATCH;
            const int GOOD_LENGTH = 8, MAX_LAZY = 16, NICE_LENGTH = 128, MAX_CHAIN = 128;      // configuration_table[6]
            const int LIT_BUFSIZE = 1 << 14, SYM_END = LIT_BUFSIZE - 1;                        // symbols per block
            const int L_CODES = 286, D_CODES = 30, BL_CODES = 19, HEAP_SIZE = 2 * L_CODES + 1, LITERALS = 256, END_BLOCK = 256;
            const int MAX_BITS = 15, MAX_BL_BITS = 7, REP_3_6 = 16, REPZ_3_10 = 17, REPZ_11_138 = 18;

            static readonly int[] ExtraL = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
            static readonly int[] ExtraD = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
            static readonly int[] ExtraBl = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7 };
            static readonly byte[] BlOrder = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            static readonly ushort[] StaticLCode = new ushort[L_CODES + 2], StaticLLen = new ushort[L_CODES + 2];
            static readonly ushort[] StaticDCode = new ushort[D_CODES], StaticDLen = new ushort[D_CODES];
            static readonly byte[] DistCode = new byte[512], LengthCode = new byte[256];
            static readonly int[] BaseLength = new int[29], BaseDist = new int[D_CODES];

            static Deflater()
            {   // tr_static_init
                int length = 0, code;
                for (code = 0; code < 28; code++)
                {
                    BaseLength[code] = length;
                    for (int n = 0; n < (1 << ExtraL[code]); n++) LengthCode[length++] = (byte)code;
                }
                LengthCode[length - 1] = (byte)code;
                int dist = 0;
                for (code = 0; code < 16; code++)
                {
                    BaseDist[code] = dist;
                    for (int n = 0; n < (1 << ExtraD[code]); n++) DistCode[dist++] = (byte)code;
                }
                dist >>= 7;
                for (; code < D_CODES; code++)
                {
                    BaseDist[code] = dist << 7;
                    for (int n = 0; n < (1 << (ExtraD[code] - 7)); n++) DistCode[256 + dist++] = (byte)code;
                }
                var blCount = new ushort[MAX_BITS + 1];
                int i = 0;
                while (i <= 143) { StaticLLen[i++] = 8; blCount[8]++; }
                while (i <= 255) { StaticLLen[i++] = 9; blCount[9]++; }
                while (i <= 279) { StaticLLen[i++] = 7; blCount[7]++; }
                while (i <= 287) { StaticLLen[i++] = 8; blCount[8]++; }
                GenCodes(StaticLCode, StaticLLen, L_CODES + 1, blCount);
                for (i = 0; i < D_CODES; i++) { StaticDLen[i] = 5; StaticDCode[i] = (ushort)BiReverse((uint)i, 5); }
            }

            static uint BiReverse(uint code, int len)
            {
                uint res = 0;
                do { res |= code & 1; code >>= 1; res <<= 1; } while (--len > 0);
                return res >> 1;
            }

            static void GenCodes(ushort[] fc, ushort[] dl, int maxCode, ushort[] blCount)
            {
                var next = new ushort[MAX_BITS + 1];
                uint code = 0;
                for (int bits = 1; bits <= MAX_BITS; bits++) { code = (code + blCount[bits - 1]) << 1; next[bits] = (ushort)code; }
                for (int n = 0; n <= maxCode; n++)
                {
                    int len = dl[n];
                    if (len == 0) continue;
                    fc[n] = (ushort)BiReverse(next[len]++, len);
                }
            }

            // ---- input / output
            readonly byte[] _in; int _inPos;
            byte[] _out = new byte[1 << 14]; int _outLen;
            ulong _biBuf; int _biValid;

            // ---- deflate state
            readonly byte[] window = new byte[WINDOW_SIZE + 8];
            readonly ushort[] prev = new ushort[W_SIZE], head = new ushort[HASH_SIZE];
            int ins_h, strstart, lookahead, match_start, match_length = MIN_MATCH - 1, prev_length = MIN_MATCH - 1, prev_match, insert;
            long block_start; bool match_available;
            long high_water;

            // ---- trees state (Freq / Code share a word, Dad / Len share a word, as in ct_data)
            readonly ushort[] lFc = new ushort[HEAP_SIZE], lDl = new ushort[HEAP_SIZE];
            readonly ushort[] dFc = new ushort[2 * D_CODES + 1], dDl = new ushort[2 * D_CODES + 1];
            readonly ushort[] blFc = new ushort[2 * BL_CODES + 1], blDl = new ushort[2 * BL_CODES + 1];
            int lMaxCode, dMaxCode, blMaxCode;
            readonly ushort[] bl_count = new ushort[MAX_BITS + 1];
            readonly int[] heap = new int[2 * L_CODES + 1]; int heap_len, heap_max;
            readonly byte[] depth = new byte[2 * L_CODES + 1];
            readonly ushort[] symDist = new ushort[LIT_BUFSIZE]; readonly byte[] symLc = new byte[LIT_BUFSIZE]; int sym_next;
            long opt_len, static_len;

            public Deflater(byte[] data) { _in = data; }

            void PutByte(int b)
            {
                if (_outLen == _out.Length) Array.Resize(ref _out, _out.Length * 2);
                _out[_outLen++] = (byte)b;
            }
            void SendBits(int value, int length)
            {
                _biBuf |= (ulong)(uint)value << _biValid; _biValid += length;
                while (_biValid >= 8) { PutByte((int)(_biBuf & 0xFF)); _biBuf >>= 8; _biValid -= 8; }
            }
            void BiWindup()
            {
                if (_biValid > 0) PutByte((int)(_biBuf & 0xFF));
                _biBuf = 0; _biValid = 0;
            }

            public byte[] Run()
            {
                PutByte(0x78); PutByte(0x9C);
                InitBlock();
                DeflateSlow();
                uint ad = Adler32(_in, 0, _in.Length);
                PutByte((int)(ad >> 24)); PutByte((int)(ad >> 16) & 0xFF); PutByte((int)(ad >> 8) & 0xFF); PutByte((int)ad & 0xFF);
                var r = new byte[_outLen]; Buffer.BlockCopy(_out, 0, r, 0, _outLen);
                return r;
            }

            // ------------------------------------------------------------------------------------ deflate.c

            void SlideHash()
            {
                for (int n = 0; n < HASH_SIZE; n++) { int m = head[n]; head[n] = (ushort)(m >= W_SIZE ? m - W_SIZE : 0); }
                for (int n = 0; n < W_SIZE; n++) { int m = prev[n]; prev[n] = (ushort)(m >= W_SIZE ? m - W_SIZE : 0); }
            }

            void FillWindow()
            {
                do
                {
                    int more = WINDOW_SIZE - lookahead - strstart;
                    if (strstart >= W_SIZE + MAX_DIST)
                    {
                        Buffer.BlockCopy(window, W_SIZE, window, 0, W_SIZE - more);
                        match_start -= W_SIZE; strstart -= W_SIZE; block_start -= W_SIZE;
                        if (insert > strstart) insert = strstart;
                        SlideHash();
                        more += W_SIZE;
                    }
                    if (_inPos >= _in.Length) break;
                    int n = Math.Min(_in.Length - _inPos, more);
                    Buffer.BlockCopy(_in, _inPos, window, strstart + lookahead, n);
                    _inPos += n; lookahead += n;
                    if (lookahead + insert >= MIN_MATCH)
                    {
                        int str = strstart - insert;
                        ins_h = window[str];
                        ins_h = ((ins_h << HASH_SHIFT) ^ window[str + 1]) & HASH_MASK;
                        while (insert != 0)
                        {
                            ins_h = ((ins_h << HASH_SHIFT) ^ window[str + MIN_MATCH - 1]) & HASH_MASK;
                            prev[str & W_MASK] = head[ins_h];
                            head[ins_h] = (ushort)str;
                            str++; insert--;
                            if (lookahead + insert < MIN_MATCH) break;
                        }
                    }
                } while (lookahead < MIN_LOOKAHEAD && _inPos < _in.Length);
                // the WIN_INIT bytes after the data are zeroed so that longest_match reads the same bytes every time
                if (high_water < WINDOW_SIZE)
                {
                    long curr = strstart + (long)lookahead, init;
                    if (high_water < curr)
                    {
                        init = WINDOW_SIZE - curr; if (init > WIN_INIT) init = WIN_INIT;
                        Array.Clear(window, (int)curr, (int)init);
                        high_water = curr + init;
                    }
                    else if (high_water < curr + WIN_INIT)
                    {
                        init = curr + WIN_INIT - high_water;
                        if (init > WINDOW_SIZE - high_water) init = WINDOW_SIZE - high_water;
                        Array.Clear(window, (int)high_water, (int)init);
                        high_water += init;
                    }
                }
            }

            int InsertString(int str)
            {
                ins_h = ((ins_h << HASH_SHIFT) ^ window[str + MIN_MATCH - 1]) & HASH_MASK;
                int h = prev[str & W_MASK] = head[ins_h];
                head[ins_h] = (ushort)str;
                return h;
            }

            int LongestMatch(int cur_match)
            {
                int chain_length = MAX_CHAIN, scan = strstart, best_len = prev_length, nice_match = NICE_LENGTH;
                int limit = strstart > MAX_DIST ? strstart - MAX_DIST : 0;
                int strend = strstart + MAX_MATCH;
                byte scan_end1 = window[scan + best_len - 1], scan_end = window[scan + best_len];
                if (prev_length >= GOOD_LENGTH) chain_length >>= 2;
                if (nice_match > lookahead) nice_match = lookahead;
                byte[] w = window;
                do
                {
                    int match = cur_match;
                    if (w[match + best_len] != scan_end || w[match + best_len - 1] != scan_end1 || w[match] != w[scan] || w[match + 1] != w[scan + 1]) continue;
                    int sc = scan + 2, mt = match + 2;
                    while (true)
                    {
                        bool mismatch = false;
                        for (int k = 0; k < 8; k++) { sc++; mt++; if (w[sc] != w[mt]) { mismatch = true; break; } }
                        if (mismatch || sc >= strend) break;
                    }
                    int len = MAX_MATCH - (strend - sc);
                    if (len > best_len)
                    {
                        match_start = cur_match;
                        best_len = len;
                        if (len >= nice_match) break;
                        scan_end1 = w[scan + best_len - 1];
                        scan_end = w[scan + best_len];
                    }
                } while ((cur_match = prev[cur_match & W_MASK]) > limit && --chain_length != 0);
                return best_len <= lookahead ? best_len : lookahead;
            }

            bool TallyLit(int c)
            {
                symDist[sym_next] = 0; symLc[sym_next++] = (byte)c;
                lFc[c]++;
                return sym_next == SYM_END;
            }
            bool TallyDist(int distance, int length)
            {
                symDist[sym_next] = (ushort)distance; symLc[sym_next++] = (byte)length;
                int dist = distance - 1;
                lFc[LengthCode[length] + LITERALS + 1]++;
                dFc[dist < 256 ? DistCode[dist] : DistCode[256 + (dist >> 7)]]++;
                return sym_next == SYM_END;
            }

            void FlushBlockOnly(bool last)
            {
                TrFlushBlock(block_start >= 0 ? (int)block_start : -1, strstart - block_start, last);
                block_start = strstart;
            }

            void DeflateSlow()
            {
                int hash_head; bool bflush;
                for (;;)
                {
                    if (lookahead < MIN_LOOKAHEAD)
                    {
                        FillWindow();
                        if (lookahead == 0) break;
                    }
                    hash_head = 0;
                    if (lookahead >= MIN_MATCH) hash_head = InsertString(strstart);
                    prev_length = match_length; prev_match = match_start;
                    match_length = MIN_MATCH - 1;
                    if (hash_head != 0 && prev_length < MAX_LAZY && strstart - hash_head <= MAX_DIST)
                    {
                        match_length = LongestMatch(hash_head);
                        if (match_length <= 5 && match_length == MIN_MATCH && strstart - match_start > TOO_FAR) match_length = MIN_MATCH - 1;
                    }
                    if (prev_length >= MIN_MATCH && match_length <= prev_length)
                    {
                        int max_insert = strstart + lookahead - MIN_MATCH;
                        bflush = TallyDist(strstart - 1 - prev_match, prev_length - MIN_MATCH);
                        lookahead -= prev_length - 1;
                        prev_length -= 2;
                        do
                        {
                            if (++strstart <= max_insert) InsertString(strstart);
                        } while (--prev_length != 0);
                        match_available = false;
                        match_length = MIN_MATCH - 1;
                        strstart++;
                        if (bflush) FlushBlockOnly(false);
                    }
                    else if (match_available)
                    {
                        bflush = TallyLit(window[strstart - 1]);
                        if (bflush) FlushBlockOnly(false);
                        strstart++; lookahead--;
                    }
                    else
                    {
                        match_available = true;
                        strstart++; lookahead--;
                    }
                }
                if (match_available) { TallyLit(window[strstart - 1]); match_available = false; }
                insert = strstart < MIN_MATCH - 1 ? strstart : MIN_MATCH - 1;
                FlushBlockOnly(true);
            }

            // ------------------------------------------------------------------------------------ trees.c

            void InitBlock()
            {
                for (int n = 0; n < L_CODES; n++) lFc[n] = 0;
                for (int n = 0; n < D_CODES; n++) dFc[n] = 0;
                for (int n = 0; n < BL_CODES; n++) blFc[n] = 0;
                lFc[END_BLOCK] = 1;
                opt_len = static_len = 0;
                sym_next = 0;
            }

            bool Smaller(ushort[] fc, int n, int m) => fc[n] < fc[m] || fc[n] == fc[m] && depth[n] <= depth[m];

            void PqDownHeap(ushort[] fc, int k)
            {
                int v = heap[k], j = k << 1;
                while (j <= heap_len)
                {
                    if (j < heap_len && Smaller(fc, heap[j + 1], heap[j])) j++;
                    if (Smaller(fc, v, heap[j])) break;
                    heap[k] = heap[j]; k = j;
                    j <<= 1;
                }
                heap[k] = v;
            }

            void GenBitlen(ushort[] fc, ushort[] dl, int maxCode, ushort[] stLen, int[] extra, int baseIndex, int maxLength)
            {
                int h, n, m, bits, xbits, overflow = 0;
                for (bits = 0; bits <= MAX_BITS; bits++) bl_count[bits] = 0;
                dl[heap[heap_max]] = 0;
                for (h = heap_max + 1; h < HEAP_SIZE; h++)
                {
                    n = heap[h];
                    bits = dl[dl[n]] + 1;
                    if (bits > maxLength) { bits = maxLength; overflow++; }
                    dl[n] = (ushort)bits;
                    if (n > maxCode) continue;
                    bl_count[bits]++;
                    xbits = 0;
                    if (n >= baseIndex) xbits = extra[n - baseIndex];
                    int f = fc[n];
                    opt_len += (long)f * (bits + xbits);
                    if (stLen != null) static_len += (long)f * (stLen[n] + xbits);
                }
                if (overflow == 0) return;
                do
                {
                    bits = maxLength - 1;
                    while (bl_count[bits] == 0) bits--;
                    bl_count[bits]--;
                    bl_count[bits + 1] += 2;
                    bl_count[maxLength]--;
                    overflow -= 2;
                } while (overflow > 0);
                for (bits = maxLength; bits != 0; bits--)
                {
                    n = bl_count[bits];
                    while (n != 0)
                    {
                        m = heap[--h];
                        if (m > maxCode) continue;
                        if (dl[m] != bits)
                        {
                            opt_len += ((long)bits - dl[m]) * fc[m];
                            dl[m] = (ushort)bits;
                        }
                        n--;
                    }
                }
            }

            int BuildTree(ushort[] fc, ushort[] dl, ushort[] stLen, int[] extra, int baseIndex, int elems, int maxLength)
            {
                int n, m, max_code = -1, node;
                heap_len = 0; heap_max = HEAP_SIZE;
                for (n = 0; n < elems; n++)
                {
                    if (fc[n] != 0) { heap[++heap_len] = max_code = n; depth[n] = 0; }
                    else dl[n] = 0;
                }
                while (heap_len < 2)
                {
                    node = heap[++heap_len] = max_code < 2 ? ++max_code : 0;
                    fc[node] = 1;
                    depth[node] = 0;
                    opt_len--; if (stLen != null) static_len -= stLen[node];
                }
                for (n = heap_len / 2; n >= 1; n--) PqDownHeap(fc, n);
                node = elems;
                do
                {
                    n = heap[1]; heap[1] = heap[heap_len--]; PqDownHeap(fc, 1);      // pqremove
                    m = heap[1];
                    heap[--heap_max] = n;
                    heap[--heap_max] = m;
                    fc[node] = (ushort)(fc[n] + fc[m]);
                    depth[node] = (byte)((depth[n] >= depth[m] ? depth[n] : depth[m]) + 1);
                    dl[n] = dl[m] = (ushort)node;
                    heap[1] = node++;
                    PqDownHeap(fc, 1);
                } while (heap_len >= 2);
                heap[--heap_max] = heap[1];
                GenBitlen(fc, dl, max_code, stLen, extra, baseIndex, maxLength);
                GenCodes(fc, dl, max_code, bl_count);
                return max_code;
            }

            void ScanTree(ushort[] dl, int max_code)
            {
                int prevlen = -1, curlen, nextlen = dl[0], count = 0, max_count = 7, min_count = 4;
                if (nextlen == 0) { max_count = 138; min_count = 3; }
                dl[max_code + 1] = 0xFFFF;
                for (int n = 0; n <= max_code; n++)
                {
                    curlen = nextlen; nextlen = dl[n + 1];
                    if (++count < max_count && curlen == nextlen) continue;
                    else if (count < min_count) blFc[curlen] += (ushort)count;
                    else if (curlen != 0)
                    {
                        if (curlen != prevlen) blFc[curlen]++;
                        blFc[REP_3_6]++;
                    }
                    else if (count <= 10) blFc[REPZ_3_10]++;
                    else blFc[REPZ_11_138]++;
                    count = 0; prevlen = curlen;
                    if (nextlen == 0) { max_count = 138; min_count = 3; }
                    else if (curlen == nextlen) { max_count = 6; min_count = 3; }
                    else { max_count = 7; min_count = 4; }
                }
            }

            void SendTree(ushort[] dl, int max_code)
            {
                int prevlen = -1, curlen, nextlen = dl[0], count = 0, max_count = 7, min_count = 4;
                if (nextlen == 0) { max_count = 138; min_count = 3; }
                for (int n = 0; n <= max_code; n++)
                {
                    curlen = nextlen; nextlen = dl[n + 1];
                    if (++count < max_count && curlen == nextlen) continue;
                    else if (count < min_count)
                    {
                        do { SendBits(blFc[curlen], blDl[curlen]); } while (--count != 0);
                    }
                    else if (curlen != 0)
                    {
                        if (curlen != prevlen) { SendBits(blFc[curlen], blDl[curlen]); count--; }
                        SendBits(blFc[REP_3_6], blDl[REP_3_6]); SendBits(count - 3, 2);
                    }
                    else if (count <= 10) { SendBits(blFc[REPZ_3_10], blDl[REPZ_3_10]); SendBits(count - 3, 3); }
                    else { SendBits(blFc[REPZ_11_138], blDl[REPZ_11_138]); SendBits(count - 11, 7); }
                    count = 0; prevlen = curlen;
                    if (nextlen == 0) { max_count = 138; min_count = 3; }
                    else if (curlen == nextlen) { max_count = 6; min_count = 3; }
                    else { max_count = 7; min_count = 4; }
                }
            }

            int BuildBlTree()
            {
                ScanTree(lDl, lMaxCode);
                ScanTree(dDl, dMaxCode);
                blMaxCode = BuildTree(blFc, blDl, null, ExtraBl, 0, BL_CODES, MAX_BL_BITS);
                int max_blindex;
                for (max_blindex = BL_CODES - 1; max_blindex >= 3; max_blindex--)
                    if (blDl[BlOrder[max_blindex]] != 0) break;
                opt_len += 3 * ((long)max_blindex + 1) + 5 + 5 + 4;
                return max_blindex;
            }

            void CompressBlock(ushort[] lCode, ushort[] lLen, ushort[] dCode, ushort[] dLen)
            {
                for (int sx = 0; sx < sym_next; sx++)
                {
                    int dist = symDist[sx], lc = symLc[sx];
                    if (dist == 0) SendBits(lCode[lc], lLen[lc]);
                    else
                    {
                        int code = LengthCode[lc];
                        SendBits(lCode[code + LITERALS + 1], lLen[code + LITERALS + 1]);
                        int extra = ExtraL[code];
                        if (extra != 0) { lc -= BaseLength[code]; SendBits(lc, extra); }
                        dist--;
                        code = dist < 256 ? DistCode[dist] : DistCode[256 + (dist >> 7)];
                        SendBits(dCode[code], dLen[code]);
                        extra = ExtraD[code];
                        if (extra != 0) { dist -= BaseDist[code]; SendBits(dist, extra); }
                    }
                }
                SendBits(lCode[END_BLOCK], lLen[END_BLOCK]);
            }

            void TrFlushBlock(int buf, long stored_len, bool last)
            {
                lMaxCode = BuildTree(lFc, lDl, StaticLLen, ExtraL, LITERALS + 1, L_CODES, MAX_BITS);
                dMaxCode = BuildTree(dFc, dDl, StaticDLen, ExtraD, 0, D_CODES, MAX_BITS);
                int max_blindex = BuildBlTree();
                long opt_lenb = (opt_len + 3 + 7) >> 3, static_lenb = (static_len + 3 + 7) >> 3;
                if (static_lenb <= opt_lenb) opt_lenb = static_lenb;
                if (stored_len + 4 <= opt_lenb && buf >= 0)
                {   // _tr_stored_block
                    SendBits(last ? 1 : 0, 3);
                    BiWindup();
                    PutByte((int)(stored_len & 0xFF)); PutByte((int)((stored_len >> 8) & 0xFF));
                    PutByte((int)(~stored_len & 0xFF)); PutByte((int)((~stored_len >> 8) & 0xFF));
                    for (long i = 0; i < stored_len; i++) PutByte(window[buf + i]);
                }
                else if (static_lenb == opt_lenb)
                {
                    SendBits((1 << 1) + (last ? 1 : 0), 3);
                    CompressBlock(StaticLCode, StaticLLen, StaticDCode, StaticDLen);
                }
                else
                {
                    SendBits((2 << 1) + (last ? 1 : 0), 3);
                    int lcodes = lMaxCode + 1, dcodes = dMaxCode + 1, blcodes = max_blindex + 1;
                    SendBits(lcodes - 257, 5); SendBits(dcodes - 1, 5); SendBits(blcodes - 4, 4);
                    for (int rank = 0; rank < blcodes; rank++) SendBits(blDl[BlOrder[rank]], 3);
                    SendTree(lDl, lcodes - 1);
                    SendTree(dDl, dcodes - 1);
                    CompressBlock(lFc, lDl, dFc, dDl);
                }
                InitBlock();
                if (last) BiWindup();
            }
        }
    }
}

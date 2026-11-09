using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    // Reflex archives (.rxdata / .rxtrack version 5, core/plan/RXARCHIVE.md) through the core's rx_archive_* exports (core/api/rx_archive.c):
    // the client reads and writes the same container the core's file system mounts, with the core's own zstd and checks. No session is
    // needed; every call is thread safe (the core's archive code locks what it shares). Calls always go to the MAIN image of the core (an
    // archive handle belongs to the image that opened it; a simulation worker on the arena image reads through the main one too).
    public static class RxArchiveNative
    {
        public const uint KindData = 0x41544144u, KindTrack = 0x4B415254u;
        public const uint StoreFlag = 1;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr OpenFn([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void CloseFn(IntPtr a);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint KindFn(IntPtr a);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr MetaFn(IntPtr a, out long length);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CountFn(IntPtr a);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr PathFn(IntPtr a, int index);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate long SizeFn(IntPtr a, int index);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FindFn(IntPtr a, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int ReadFn(IntPtr a, int index, ulong offset, byte* dst, ulong n);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate long ListFn(IntPtr a, byte* dst, long cap, long* sizes, int nsizes, byte* kinds);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int EntryShaFn(IntPtr a, int index, byte* hex);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int AddRefFn(IntPtr w, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, byte* data, ulong n);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate long DedupedFn(IntPtr w, out long pooled);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr CreateFn([MarshalAs(UnmanagedType.LPUTF8Str)] string path, uint kind, int level);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int AddFn(IntPtr w, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, byte* data, ulong n, uint flags);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int SetMetaFn(IntPtr w, byte* json, long length);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate long WrittenFn(IntPtr w, out long raw);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FinishFn(IntPtr w);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr ErrorFn();

        static readonly RxNativeLoader.Fn<OpenFn> open = new RxNativeLoader.Fn<OpenFn>("rx_archive_open");
        static readonly RxNativeLoader.Fn<CloseFn> close = new RxNativeLoader.Fn<CloseFn>("rx_archive_close");
        static readonly RxNativeLoader.Fn<KindFn> kind = new RxNativeLoader.Fn<KindFn>("rx_archive_kind");
        static readonly RxNativeLoader.Fn<MetaFn> meta = new RxNativeLoader.Fn<MetaFn>("rx_archive_meta");
        static readonly RxNativeLoader.Fn<CountFn> count = new RxNativeLoader.Fn<CountFn>("rx_archive_count");
        static readonly RxNativeLoader.Fn<PathFn> path = new RxNativeLoader.Fn<PathFn>("rx_archive_path");
        static readonly RxNativeLoader.Fn<SizeFn> size = new RxNativeLoader.Fn<SizeFn>("rx_archive_size");
        static readonly RxNativeLoader.Fn<FindFn> find = new RxNativeLoader.Fn<FindFn>("rx_archive_find");
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_archive_read");
        static readonly RxNativeLoader.Fn<ListFn> list = new RxNativeLoader.Fn<ListFn>("rx_archive_list");
        static readonly RxNativeLoader.Fn<EntryShaFn> entrySha = new RxNativeLoader.Fn<EntryShaFn>("rx_archive_entry_sha");
        static readonly RxNativeLoader.Fn<CountFn> version = new RxNativeLoader.Fn<CountFn>("rx_archive_version");
        static readonly RxNativeLoader.Fn<AddRefFn> addRef = new RxNativeLoader.Fn<AddRefFn>("rx_archive_add_ref");
        static readonly RxNativeLoader.Fn<DedupedFn> deduped = new RxNativeLoader.Fn<DedupedFn>("rx_archive_deduped");
        static readonly RxNativeLoader.Fn<CreateFn> create = new RxNativeLoader.Fn<CreateFn>("rx_archive_create");
        static readonly RxNativeLoader.Fn<AddFn> add = new RxNativeLoader.Fn<AddFn>("rx_archive_add");
        static readonly RxNativeLoader.Fn<SetMetaFn> setMeta = new RxNativeLoader.Fn<SetMetaFn>("rx_archive_set_meta");
        static readonly RxNativeLoader.Fn<WrittenFn> written = new RxNativeLoader.Fn<WrittenFn>("rx_archive_written");
        static readonly RxNativeLoader.Fn<FinishFn> finish = new RxNativeLoader.Fn<FinishFn>("rx_archive_finish");
        static readonly RxNativeLoader.Fn<CloseFn> abort = new RxNativeLoader.Fn<CloseFn>("rx_archive_abort");
        static readonly RxNativeLoader.Fn<ErrorFn> error = new RxNativeLoader.Fn<ErrorFn>("rx_archive_error");

        /// The delegate of the main image whatever the calling thread's image (RxNativeLoader.ThreadImage).
        static T OnMain<T>(RxNativeLoader.Fn<T> fn) where T : Delegate
        {
            int image = RxNativeLoader.ThreadImage;
            if (image == 0) return fn.Get();
            RxNativeLoader.ThreadImage = 0;
            try { return fn.Get(); } finally { RxNativeLoader.ThreadImage = image; }
        }

        /// The library exports the archive API (a core built before it does not).
        public static bool Available
        {
            get { try { OnMain(open); return true; } catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { return false; } }
        }

        public static string LastError() => Marshal.PtrToStringUTF8(OnMain(error)()) ?? "";
        static string Utf8(IntPtr p) => p == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(p);

        public static IntPtr Open(string file) => OnMain(open)(file);
        public static void Close(IntPtr a) { if (a != IntPtr.Zero) OnMain(close)(a); }
        public static uint Kind(IntPtr a) => OnMain(kind)(a);
        public static string Meta(IntPtr a)
        {
            IntPtr p = OnMain(meta)(a, out long n);
            if (p == IntPtr.Zero || n <= 0) return "";
            var b = new byte[n]; Marshal.Copy(p, b, 0, (int)n);
            return Encoding.UTF8.GetString(b);
        }
        public static int Count(IntPtr a) => OnMain(count)(a);
        public static string Path(IntPtr a, int index) => Utf8(OnMain(path)(a, index));
        public static long Size(IntPtr a, int index) => OnMain(size)(a, index);
        public static int Find(IntPtr a, string p) => OnMain(find)(a, p);
        /// n bytes at offset of file index into buffer[at..]: false (LastError says why) on a range, read, decode or checksum failure.
        public static unsafe bool Read(IntPtr a, int index, long offset, byte[] buffer, int at, int n)
        {
            if (buffer == null || at < 0 || n < 0 || at > buffer.Length - n || offset < 0) return false;
            if (n == 0) return true;
            fixed (byte* p = buffer) return OnMain(read)(a, index, (ulong)offset, p + at, (ulong)n) == 0;
        }

        /// Every path, size and kind (true: a pool reference, RXARCHIVE.md "Dedupe") in two calls (rx_archive_list).
        public static unsafe void List(IntPtr a, out string[] paths, out long[] sizes, out bool[] refs)
        {
            int n = Count(a);
            paths = new string[n]; sizes = new long[n]; refs = new bool[n];
            var kinds = new byte[n];
            var fn = OnMain(list);
            long need = fn(a, null, 0, null, 0, null);
            if (need < 0) throw new InvalidOperationException("rx_archive_list: bad archive");
            var bytes = new byte[need];
            fixed (byte* b = bytes) fixed (long* s = sizes) fixed (byte* kp = kinds) fn(a, b, need, s, n, kp);
            for (int i = 0; i < n; i++) refs[i] = kinds[i] != 0;
            int at = 0, k = 0;
            for (int i = 0; i <= bytes.Length && k < n; i++)
                if (i == bytes.Length || bytes[i] == (byte)'\n') { paths[k++] = Encoding.UTF8.GetString(bytes, at, i - at); at = i + 1; }
        }

        /// An entry's SHA-256 (64 hex digits) and kind: 0 none (container version 1), 1 its bytes are here, 2 a pool reference (Pool/<hex>).
        public static unsafe int EntrySha(IntPtr a, int index, out string hex)
        {
            var b = new byte[65];
            int k; fixed (byte* p = b) k = OnMain(entrySha)(a, index, p);
            hex = k == 0 ? null : Encoding.ASCII.GetString(b, 0, 64);
            return k;
        }
        /// The container version: 1, or 2 (SHA-256 per entry, dedupe, pool references).
        public static int Version(IntPtr a) => OnMain(version)(a);
        /// A pool reference: path's bytes are the root's Pool/<their SHA-256>, not stored in this archive.
        public static unsafe bool AddRef(IntPtr w, string p, byte[] data, int n)
        {
            if (data == null || n < 0 || n > data.Length) return false;
            fixed (byte* d = data) return OnMain(addRef)(w, p, d, (ulong)n) == 0;
        }
        /// Raw bytes stored once for several paths, and left to the pool, so far.
        public static long Deduped(IntPtr w, out long pooled) => OnMain(deduped)(w, out pooled);

        public static IntPtr Create(string file, uint archiveKind, int level) => OnMain(create)(file, archiveKind, level);
        /// One file into a writer; thread safe (compression runs on the calling thread).
        public static unsafe bool Add(IntPtr w, string p, byte[] data, int n, uint flags = 0)
        {
            if (data == null || n < 0 || n > data.Length) return false;
            fixed (byte* d = data) return OnMain(add)(w, p, d, (ulong)n, flags) == 0;
        }
        public static unsafe bool SetMeta(IntPtr w, string json)
        {
            var b = Encoding.UTF8.GetBytes(json ?? "");
            fixed (byte* p = b) return OnMain(setMeta)(w, p, b.Length) == 0;
        }
        public static long Written(IntPtr w, out long raw) => OnMain(written)(w, out raw);
        public static bool Finish(IntPtr w) => OnMain(finish)(w) == 0;
        public static void Abort(IntPtr w) { if (w != IntPtr.Zero) OnMain(abort)(w); }
    }
}

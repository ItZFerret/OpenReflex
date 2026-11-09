using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace Reflex.Port.Content
{
    // The VC90 SP1 runtime the original executable links (msvcr90.dll / msvcp90.dll 9.00.30729.1), taken from the
    // install's own redistributable (Installers/vcredist_x86.exe: an IExpress CAB holding vc_red.cab, both LZX:21,
    // extracted by libreflexworld with libmspack's LZX decoder)
    // into the run-time cache. The interpreter executes these genuine DLLs; nothing is shipped with the client.
    public static class ReflexCrt
    {
        const string CrtSha = "cf420aced0d810e1d75f6811dd986f2d9fded2fbb8d61fc9a7024520c475febb";
        const string CppSha = "91fd46d7335c9990a20f215b9f6f53bc59551420a9c99ad8110ae2f9ff7598f0";
        const string Member = ".30729.01.Microsoft_VC90_CRT_x86.SP";

        /// The folder holding msvcr90.dll and msvcp90.dll (extracted on first use).
        public static string Dir
        {
            get
            {
                string dir = Path.Combine(ReflexInstall.CacheDir, "crt");
                string crt = Path.Combine(dir, "msvcr90.dll"), cpp = Path.Combine(dir, "msvcp90.dll");
                if (File.Exists(crt) && File.Exists(cpp) && Sha(File.ReadAllBytes(crt)) == CrtSha && Sha(File.ReadAllBytes(cpp)) == CppSha) return dir;
                Directory.CreateDirectory(dir);
                if (reflex_world_extract_crt(ReflexInstall.PathOf("Installers/vcredist_x86.exe"), dir) != 0)
                    throw new InvalidDataException("vcredist_x86.exe: " + Marshal.PtrToStringAnsi(reflex_world_extract_crt_error()));
                if (Sha(File.ReadAllBytes(crt)) != CrtSha || Sha(File.ReadAllBytes(cpp)) != CppSha) throw new InvalidDataException("vcredist_x86.exe: unexpected VC90 runtime version");
                return dir;
            }
        }

        static string Sha(byte[] b) { using var h = SHA256.Create(); return BitConverter.ToString(h.ComputeHash(b)).Replace("-", "").ToLowerInvariant(); }
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_extract_crt__fn(string installer, string outDir);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_extract_crt__fn> reflex_world_extract_crt__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_extract_crt__fn>("reflex_world_extract_crt");
        static int reflex_world_extract_crt(string installer, string outDir) => reflex_world_extract_crt__p.Get()(installer, outDir);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_extract_crt_error__fn();
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_extract_crt_error__fn> reflex_world_extract_crt_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_extract_crt_error__fn>("reflex_world_extract_crt_error");
        static IntPtr reflex_world_extract_crt_error() => reflex_world_extract_crt_error__p.Get()();
    }
}

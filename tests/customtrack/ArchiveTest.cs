// TEST ONLY (w-fb-customtrack): TrackArchives (Unity/Reflex/Assets/Scripts/Content/TrackArchives.cs) without Unity - tests/customtrack/archive_test.sh.
//   ArchiveTest <work dir> [real archive]
// Every case works in <work dir>/<case>/Mods/Tracks (made fresh); nothing outside it is written.
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using Reflex.Port.Content;

static class ArchiveTest
{
    static int bad, ok;
    static void Check(bool c, string what) { if (c) { ok++; Console.WriteLine("ok: " + what); } else { bad++; Console.WriteLine("FAIL: " + what); } }
    static string work;

    static string Tracks(string name)
    {
        string d = Path.Combine(work, name, "Mods", "Tracks");
        if (Directory.Exists(Path.Combine(work, name))) Directory.Delete(Path.Combine(work, name), true);
        Directory.CreateDirectory(d);
        return d;
    }
    static TrackArchives.Job Install(string archive, string root)
    {
        var j = new TrackArchives.Job { archive = archive, kind = TrackArchives.Sniff(archive) };
        TrackArchives.Install(j, root);
        Console.WriteLine("   " + Path.GetFileName(archive) + " (" + TrackArchives.Name(j.kind) + "): " + (j.error ?? "-> " + j.folder));
        return j;
    }
    static void Set(string dir, string stem, params string[] exts)
    {
        Directory.CreateDirectory(dir);
        foreach (var e in exts) File.WriteAllText(Path.Combine(dir, stem + ".dx9." + e), stem + " " + e);
    }
    static string Zip(string path, string from)
    {
        if (File.Exists(path)) File.Delete(path);
        ZipFile.CreateFromDirectory(from, path);
        return path;
    }
    static int Tar(string cwd, params string[] args)
    {
        var psi = new ProcessStartInfo("/usr/bin/tar") { UseShellExecute = false, WorkingDirectory = cwd, RedirectStandardError = true };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi); p.StandardError.ReadToEnd(); p.WaitForExit(); return p.ExitCode;
    }
    static string Sha(string f) { using var s = SHA256.Create(); using var fs = File.OpenRead(f); return Convert.ToHexString(s.ComputeHash(fs)); }
    static bool HasTrack(string folder, string stem) => new[] { "level", "database", "package", "scene" }.All(e => File.Exists(Path.Combine(folder, stem + ".dx9." + e)));

    static int Main(string[] args)
    {
        work = Path.GetFullPath(args[0]); string real = args.Length > 1 && args[1].Length > 0 ? args[1] : null;
        if (Directory.Exists(work)) Directory.Delete(work, true);
        Directory.CreateDirectory(work);
        TrackArchives.Log = s => Console.WriteLine("   log " + s);

        // 1. the first bytes, not the name
        Check(TrackArchives.Sniff(Encoding.ASCII.GetBytes("PK\u0003\u0004xxxx"), 8) == TrackArchives.Kind.Zip, "zip by PK 03 04");
        Check(TrackArchives.Sniff(new byte[] { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00, 0xCF }, 8) == TrackArchives.Kind.Rar4, "RAR 4 by Rar! 1A 07 00");
        Check(TrackArchives.Sniff(new byte[] { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 }, 8) == TrackArchives.Kind.Rar5, "RAR 5 by Rar! 1A 07 01 00");
        Check(TrackArchives.Sniff(new byte[] { 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C, 0, 4 }, 8) == TrackArchives.Kind.SevenZip, "7z by 37 7A BC AF 27 1C");
        Check(TrackArchives.Sniff(Encoding.ASCII.GetBytes("BXML...."), 8) == TrackArchives.Kind.None && TrackArchives.Sniff(new byte[3], 3) == TrackArchives.Kind.None, "a track file / a short file is no archive");

        // 2. a zip with the track in a folder of its own (and the desktops' junk): flattened into <archive name>/, marked, not installed twice
        {
            string root = Tracks("zip"), src = Path.Combine(work, "zip", "src");
            Set(Path.Combine(src, "My Track"), "Beta_Nat_Track_Slot_2", "level", "database", "package", "scene");
            File.WriteAllText(Path.Combine(src, "My Track", "readme.txt"), "hi");
            Directory.CreateDirectory(Path.Combine(src, "__MACOSX")); File.WriteAllText(Path.Combine(src, "__MACOSX", "._x"), "junk");
            string a = Zip(Path.Combine(root, "1200-my-track.zip"), src);
            var j = Install(a, root);
            Check(j.error == null && j.folder == Path.Combine(root, "1200-my-track") && HasTrack(j.folder, "Beta_Nat_Track_Slot_2"), "zip: installed into Mods/Tracks/1200-my-track/, the single top folder flattened");
            Check(j.folder != null && File.Exists(Path.Combine(j.folder, TrackArchives.MarkerName)) && TrackArchives.InstalledFolder(a) == j.folder, "zip: marked as installed from the archive");
            Check(File.Exists(a), "zip: the archive is kept");
            Check(!Directory.Exists(Path.Combine(root, TrackArchives.StagingName)), "zip: no staging folder left");
            var j2 = Install(a, root);
            Check(j2.error == null && j2.folder == j.folder && Directory.GetDirectories(root).Length == 1, "zip: a second install is the first one's folder");
            Directory.Delete(j.folder, true);
            var j3 = Install(a, root);
            Check(j3.error == null && j3.folder == Path.Combine(root, "1200-my-track"), "zip: the folder deleted: installed again");
            // a folder of that name that is not the archive's: another name
            File.AppendAllText(a, "");   // (same size: still the archive)
            string other = Path.Combine(root, "x"); Directory.CreateDirectory(other);
            string a2 = Path.Combine(root, "x.zip"); File.Copy(a, a2);
            var j4 = Install(a2, root);
            Check(j4.error == null && j4.folder == Path.Combine(root, "x (2)"), "zip: an existing folder of the name is left alone (x (2))");
        }

        // 3. a 7z named .zip (the name says nothing): the system's tar
        if (File.Exists("/usr/bin/tar"))
        {
            string root = Tracks("7z"), src = Path.Combine(work, "7z", "src");
            Set(src, "Beta_Sx_Track_Slot_3", "level", "database", "package", "scene");
            string a = Path.Combine(root, "sx-track.zip");
            int rc = Tar(src, "--format", "7zip", "-cf", a, "Beta_Sx_Track_Slot_3.dx9.level", "Beta_Sx_Track_Slot_3.dx9.database", "Beta_Sx_Track_Slot_3.dx9.package", "Beta_Sx_Track_Slot_3.dx9.scene");
            Check(rc == 0 && TrackArchives.Sniff(a) == TrackArchives.Kind.SevenZip, "7z: made by /usr/bin/tar, named .zip, sniffed as 7z");
            var j = Install(a, root);
            Check(j.error == null && HasTrack(j.folder ?? "", "Beta_Sx_Track_Slot_3"), "7z named .zip: installed");
            // a non-ASCII name (the copy path for Windows' tar)
            string u = Path.Combine(root, "Zoë's piste.7z"); File.Copy(a, u);
            var ju = Install(u, root);
            Check(ju.error == null && ju.folder == Path.Combine(root, "Zoë's piste") && HasTrack(ju.folder, "Beta_Sx_Track_Slot_3"), "7z with a non-ASCII name: installed");
        }

        // 4. the real archive (a RAR 4 named .zip): the same files as tar -x
        if (real != null && File.Exists(real))
        {
            string root = Tracks("real"), ref_ = Path.Combine(work, "real", "ref");
            Directory.CreateDirectory(ref_);
            string a = Path.Combine(root, Path.GetFileName(real)); File.Copy(real, a);
            Check(Tar(ref_, "-xf", a) == 0, "real: tar -xf (the reference)");
            var kind = TrackArchives.Sniff(a);
            Console.WriteLine("   real archive " + Path.GetFileName(real) + ": " + TrackArchives.Name(kind));
            var j = Install(a, root);
            var want = Directory.GetFiles(ref_, "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(ref_, f)).OrderBy(x => x, StringComparer.Ordinal).ToList();
            bool same = j.folder != null && want.Count > 0 && want.All(r => File.Exists(Path.Combine(j.folder, r)) && Sha(Path.Combine(j.folder, r)) == Sha(Path.Combine(ref_, r)));
            Check(j.error == null && same, "real: installed into " + Path.GetFileName(j.folder ?? "?") + "/, " + want.Count + " files byte-identical to tar -x");
            // the same archive cut short: tar's reason, the archive kept
            string cut = Path.Combine(root, "cut.rar");
            using (var i = File.OpenRead(a)) using (var o = File.Create(cut)) { var buf = new byte[1 << 20]; int n = i.Read(buf, 0, buf.Length); o.Write(buf, 0, n); }
            var jc = Install(cut, root);
            Check(jc.error != null && jc.error.Contains("tar") && File.Exists(cut) && !Directory.Exists(Path.Combine(root, "cut")), "a RAR cut short: not installed, says why (" + jc.error + ")");
        }

        // 5. what is not a track
        {
            string root = Tracks("bad");
            string slip = Path.Combine(root, "slip.zip");
            using (var z = ZipFile.Open(slip, ZipArchiveMode.Create)) { var e = z.CreateEntry("../evil.txt"); using var w = new StreamWriter(e.Open()); w.Write("x"); }
            var js = Install(slip, root);
            Check(js.error != null && js.error.Contains("outside") && !File.Exists(Path.Combine(work, "bad", "Mods", "evil.txt")), "a zip naming ../evil.txt: refused");
            string empty = Path.Combine(root, "empty.zip"); File.WriteAllBytes(empty, new byte[0]);
            var je = Install(empty, root);
            Check(je.error != null && je.error.Contains("is empty"), "an empty .zip: the download did not finish");
            string junk = Path.Combine(root, "junk.rar"); File.WriteAllBytes(junk, Enumerable.Range(0, 5000).Select(i => (byte)(i * 7)).ToArray());
            var jj = Install(junk, root);
            Check(jj.error != null && jj.error.Contains("not a zip, RAR or 7z"), "random bytes named .rar: not an archive");
            string bsrc = Path.Combine(work, "bad", "bike"); Set(bsrc, "MXs", "database", "package");
            var jb = Install(Zip(Path.Combine(root, "bike.zip"), bsrc), root);
            Check(jb.error != null && jb.error.Contains("bike mod") && !Directory.Exists(Path.Combine(root, "bike")), "a bike mod's zip: no track, goes in Mods/Bikes");
            string hsrc = Path.Combine(work, "bad", "half"); Set(hsrc, "Beta_Track_Slot_4", "level", "database");
            var jh = Install(Zip(Path.Combine(root, "half.zip"), hsrc), root);
            Check(jh.error != null && jh.error.Contains("incomplete") && jh.error.Contains(".dx9.package") && jh.error.Contains(".dx9.scene"), "a zip with half a track: says which files are missing");
            string r5 = Path.Combine(root, "fake5.rar");
            File.WriteAllBytes(r5, new byte[] { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 }.Concat(new byte[200]).ToArray());
            var j5 = Install(r5, root);
            Check(j5.kind == TrackArchives.Kind.Rar5 && j5.error != null && j5.error.Contains("tar"), "a damaged RAR 5: the system's tar says why");
            Check(Directory.GetDirectories(root).Length == 0, "nothing installed from the bad archives (" + string.Join(", ", Directory.GetDirectories(root).Select(Path.GetFileName)) + ")");
        }

        // 6. the worker: Ensure starts once, Generation moves when it ends
        {
            string root = Tracks("async"), src = Path.Combine(work, "async", "src");
            Set(src, "Beta_Track_Slot_1", "level", "database", "package", "scene");
            string a = Zip(Path.Combine(root, "free.zip"), src);
            int g = TrackArchives.Generation;
            var j = TrackArchives.Ensure(a, root);
            Check(TrackArchives.Ensure(a, root) == j, "Ensure: one job per archive");
            for (int i = 0; i < 200 && j.state == "installing"; i++) Thread.Sleep(50);
            Check(j.state == "done" && TrackArchives.Generation > g && HasTrack(Path.Combine(root, "free"), "Beta_Track_Slot_1"), "Ensure: installed on a worker, Generation moved");
        }

        Console.WriteLine($"{ok} ok, {bad} failed");
        Console.WriteLine(bad == 0 ? "ARCHIVES PASS" : "ARCHIVES FAIL");
        return bad == 0 ? 0 : 1;
    }
}

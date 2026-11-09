using System;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // The custom tracks' folder rules (Content/TrackMods.cs): the kinds of the slot names, the names of the folders, the recursive scan of a
    // scratch Mods/Tracks tree (nested folders, spaces, junk, a set with files missing, two tracks of one slot, two stems in one folder,
    // loose files in Mods/Tracks, a loose archive, mod.ini, a picture as the thumbnail). The tracks' files are stand-ins (the scan's checks
    // that read them fail on them: the reasons are checked too). With REFLEX_TRACKMOD_ROOT (a Mods/Tracks folder of real tracks): every
    // complete track there is listed without a problem.
    // Run: Unity -batchmode -projectPath <copy> -executeMethod Reflex.Port.Editor.TrackModsTests.Verify -quit
    public static class TrackModsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[TRACKMODS] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static void Eq(string got, string want, string what) => Check(got == want, what + ": \"" + got + "\"" + (got == want ? "" : " (want \"" + want + "\")"));
        static void Touch(string path, int bytes = 16) { Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllBytes(path, new byte[bytes]); }
        static void Set(string dir, string stem, params string[] exts) { foreach (var e in exts) Touch(Path.Combine(dir, stem + ".dx9." + e)); }

        public static void Verify()
        {
            report.Clear(); bad = 0;
            string tmp = null;
            try
            {
                // 1. kinds of the slot names (the community's tools) and of the original level names
                Eq(TrackMods.KindOf("Beta_Track_Slot_1").kind, "FreeRide", "Beta_Track_Slot_1");
                Eq(TrackMods.KindOf("Beta_Nat_Track_Slot_4").kind, "National", "Beta_Nat_Track_Slot_4");
                Eq(TrackMods.KindOf("beta_sx_track_slot_12").kind, "Supercross", "beta_sx_track_slot_12 (any case)");
                Eq(TrackMods.KindOf("Beta_Trx_Track_Slot_1").label, "TERRACROSS  ·  FREE RIDE", "another beta kind: free-ridden, named");
                Eq(TrackMods.KindOf("NAT_MyTrack").kind, "National", "an original-style National name");
                Eq(TrackMods.KindOf("SX_Arena").kind, "Supercross", "an original-style Supercross name");
                Eq(TrackMods.KindOf("Whatever").kind, "FreeRide", "any other name: a Free Ride map");

                // 2. the names of the folders
                Eq(TrackMods.Pretty("1138-farm-14-mx"), "Farm 14 MX", "leading number dropped, words cased, MX kept");
                Eq(TrackMods.Pretty("1212-redbull-straight-rhythm-2022-freeride"), "Redbull Straight Rhythm 2022 Freeride", "a long folder name");
                Eq(TrackMods.Pretty("975-slayground"), "Slayground", "975-slayground");
                Eq(TrackMods.Pretty("rd1"), "Rd1", "rd1");
                Eq(TrackMods.Pretty("2022"), "2022", "a name of digits stays");
                Eq(TrackMods.Pretty("RedBull_Imagination"), "RedBull Imagination", "a cased word keeps its case");

                // 3. the scan
                tmp = Path.Combine(Path.GetTempPath(), "reflex-trackmods-" + Guid.NewGuid().ToString("N"));
                string root = Path.Combine(tmp, "Mods", "Tracks");
                Set(Path.Combine(root, "1138-farm-14-mx"), "Beta_Nat_Track_Slot_4", "database", "level", "package", "scene");
                Set(Path.Combine(root, "My Stuff", "Straight Rhythm"), "Beta_Track_Slot_1", "database", "level", "package", "scene");
                Set(Path.Combine(root, "Zoë's tracks", "Practice"), "BETA_TRACK_SLOT_1", "DATABASE", "LEVEL", "PACKAGE", "SCENE");
                File.WriteAllText(Path.Combine(root, "Zoë's tracks", "Practice", "mod.ini"), "name = Practice Loop\nauthor = Zoë\nthumbnail = shot.png\nkind = supercross\n");
                Touch(Path.Combine(root, "Zoë's tracks", "Practice", "shot.png"));
                Set(Path.Combine(root, "Half"), "Beta_Track_Slot_2", "database", "level");
                Set(Path.Combine(root, "Two"), "Beta_Track_Slot_3", "database", "level", "package", "scene");
                Set(Path.Combine(root, "Two"), "Beta_Sx_Track_Slot_3", "database", "level", "package", "scene");
                Touch(Path.Combine(root, "Two", "preview.jpg"));
                Set(root, "Beta_Track_Slot_9", "database", "level", "package", "scene");
                Touch(Path.Combine(root, "downloaded track.zip"));
                File.WriteAllBytes(Path.Combine(root, "573-2014-monster-energy-cup.dat"), new byte[] { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00, 0xCF, 0x90 });   // (w-fb-customtrack) a RAR by its bytes, any name
                Touch(Path.Combine(root, ".DS_Store")); Touch(Path.Combine(root, "__MACOSX", "Beta_Track_Slot_5.dx9.level")); Touch(Path.Combine(root, "1138-farm-14-mx", "._Beta_Nat_Track_Slot_4.dx9.level"));
                var mods = TrackMods.Scan(root);
                Log("found: " + string.Join(" | ", mods.Select(m => m.id + " = " + m.name + " (" + m.level + ", " + m.kind + (m.archive ? ", archive" : "") + ")")));
                Check(mods.Count == 9, "9 entries: 7 track sets and 2 archives (got " + mods.Count + ")");
                var farm = mods.FirstOrDefault(m => m.id == "1138-farm-14-mx");
                Check(farm != null && farm.name == "Farm 14 MX" && farm.kind == "National" && farm.level == "Beta_Nat_Track_Slot_4", "the National track by its folder");
                Check(farm != null && farm.problem != null && !farm.problem.StartsWith("missing"), "stand-in files: a read problem, not a missing file (" + farm?.problem + ")");
                var sr = mods.FirstOrDefault(m => m.id == "My Stuff/Straight Rhythm");
                var pr = mods.FirstOrDefault(m => m.id == "Zoë's tracks/Practice");
                Check(sr != null && pr != null && sr.level.Equals(pr.level, StringComparison.OrdinalIgnoreCase) && sr.Token != pr.Token, "two tracks of one slot: two entries, two event keys");
                Check(pr != null && pr.name == "Practice Loop" && pr.author == "Zoë" && pr.kind == "Supercross" && pr.thumbnail != null && pr.thumbnail.EndsWith("shot.png"), "mod.ini: name, author, kind, thumbnail");
                Check(pr != null && pr.File(".dx9.package") != null, "upper-case file names are found");
                var half = mods.FirstOrDefault(m => m.id == "Half");
                Check(half != null && half.problem != null && half.problem.StartsWith("missing") && half.problem.Contains("Beta_Track_Slot_2.dx9.package") && half.problem.Contains(".dx9.scene"), "a set with files missing says which (" + half?.problem + ")");
                var two = mods.Where(m => m.folder.EndsWith("Two")).ToList();
                Check(two.Count == 2 && two.All(m => m.id.StartsWith("Two+")) && two.All(m => m.name.StartsWith("Two (")) && two.All(m => m.thumbnail != null), "two stems in one folder: two tracks, the folder's picture for both");
                var loose = mods.FirstOrDefault(m => m.level == "Beta_Track_Slot_9");
                Check(loose != null && loose.id == "Beta_Track_Slot_9" && loose.name == "Beta Track Slot 9", "files right in Mods/Tracks: a track named by its stem");
                var rar = mods.FirstOrDefault(m => m.archive && m.id.EndsWith(".dat"));
                Check(rar != null && rar.problem.Contains("RAR 4 archive") && rar.problem.Contains("573-2014-monster-energy-cup/"), "an archive by its first bytes, whatever its name (" + rar?.problem + ")");
                var zip = mods.FirstOrDefault(m => m.archive && m.id.EndsWith(".zip"));
                Check(zip != null && zip.id == "downloaded track.zip" && !zip.Ok && zip.problem.Contains("extract"), "a loose archive: a note to extract it");
                Check(!mods.Any(m => m.level == "Beta_Track_Slot_5"), "__MACOSX and AppleDouble files are not tracks");

                // 4. the user's real tracks (optional)
                string real = Environment.GetEnvironmentVariable("REFLEX_TRACKMOD_ROOT");
                if (!string.IsNullOrEmpty(real) && Directory.Exists(real))
                {
                    var r = TrackMods.Scan(real);
                    foreach (var m in r) Check(m.Ok || m.archive || m.problem.StartsWith("missing"), "real track (a complete set reads) " + m.id + ": " + m.name + " " + m.kindLabel + (m.problem != null ? " (" + m.problem + ")" : ""));
                }
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally { try { if (tmp != null) Directory.Delete(tmp, true); } catch (Exception) { } }
            Log(bad == 0 ? "TRACKMODS PASS" : "TRACKMODS FAIL " + bad);
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}

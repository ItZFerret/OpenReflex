// MX vs ATV Untamed converter, packaging: the port of the Python reference converter's ux_package.py (the release form of a built track, the
// folder OPEN REFLEX loads from Mods/Tracks).
//   * the numeric BXML attr value-count flags of .database and .scene set to the stock 1 (PatchDoc / SceneRebuild), after proving that
//     the unpatched re-encode is byte-identical and that nothing but flags differs afterwards; .level and .package are byte copies
//   * the Open Reflex 'cannot be ridden' rules re-read from the written files (Rules)
//   * mod.ini (name '<MX07Tables display name> (Untamed)', author, one-line description, thumbnail, kind), preview.png and sha256.txt
// Not ported: the verify.log gate and the ux_orcheck gate of the Python tool (they are the reference's own acceptance; the test runs
// them on this port's output), --keep-flags, --name / --description / --kind (the 12 stadiums are supercross tracks).
// Deviations: the description's date is an option (null = no date: the file must not depend on the clock); the preview picture is the
// track's top-down ground bake (terrain stage) scaled to 512 px, where the reference scales a render made by its verifier.
// System only, C# 9, .NET Standard 2.1.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxPackage
    {
        public const string AUTHOR = "OPEN REFLEX Untamed import";
        /// The description after the track's name (the reference writes its own pipeline's name here; the only text of this port's
        /// mod.ini that differs from it).
        public const string CONVERTED = " from MX vs ATV Untamed (Xbox 360), converted from the player's own disc by the OPEN REFLEX Untamed import";
        public const string PREVIEW = "preview.png", MOD_INI = "mod.ini", SHA256 = "sha256.txt";
        public const int PREVIEW_WIDTH = 512;
        static readonly Regex KIND_RX = new Regex(@"^Beta_(?:([A-Za-z]+)_)?Track_Slot_?\d*$", RegexOptions.CultureInvariant);
        static readonly Regex NAME_RX = new Regex(@"^[A-Za-z0-9_]{1,60}$", RegexOptions.CultureInvariant);
        /// NUMERIC: the BXML value types every stock Krome file writes with flag 1.
        static bool Numeric(long type) => type == 3 || type == 4 || type == 5 || type == 7 || type == 8 || type == 10 || type == 11;

        // ------------------------------------------------------------------------------------------- stock attr flags
        /// patch_doc(data, patch): the document re-encoded with the numeric attr flag words 0 -> 1; n = the attrs set.
        public static byte[] PatchDoc(byte[] data, out int n, bool patch = true)
        {
            var b = new UxBxml(data);
            n = 0;
            if (patch)
                foreach (var a in b.attrs)
                    if (Numeric(a[3]) && a[2] == 0) { a[2] = 1; n++; }
            return b.ToBytes();
        }

        /// The (name bytes, chunk count, entity count, chunk documents) of every sub-scene of a .dx9.scene.
        static List<(byte[] name, uint nch, uint nent, List<byte[]> chunks)> SceneChunks(byte[] d, out uint magic)
        {
            magic = UxBin.U32LE(d, 0); int nsub = (int)UxBin.U32LE(d, 4);
            var o = new List<(byte[], uint, uint, List<byte[]>)>();
            for (int i = 0; i < nsub; i++)
            {
                int at = 8 + i * 264;
                byte[] name = UxBin.Slice(d, at, 260);
                int off = (int)UxBin.U32LE(d, at + 260);
                uint nch = UxBin.U32LE(d, off), nent = UxBin.U32LE(d, off + 4);
                int p = off + 8; var cs = new List<byte[]>();
                for (uint k = 0; k < nch; k++) { int size = 36 + (int)UxBin.U32LE(d, p + 32); cs.Add(UxBin.Slice(d, p, size)); p += size; }
                o.Add((name, nch, nent, cs));
            }
            return o;
        }

        /// scene_rebuild(data, patch): every chunk document through PatchDoc, the sub-scene table laid out again.
        public static byte[] SceneRebuild(byte[] data, out int total, bool patch = true)
        {
            total = 0;
            var subs = SceneChunks(data, out uint magic);
            var bodies = new List<byte[]>();
            foreach (var (_, nch, nent, chunks) in subs)
            {
                var w = new UxWriter(1 << 16);
                w.U32(nch).U32(nent);
                foreach (byte[] c in chunks) { w.Bytes(PatchDoc(c, out int k, patch)); total += k; }
                bodies.Add(w.ToArray());
            }
            var o = new UxWriter(data.Length + 64);
            o.U32(magic).U32((uint)subs.Count);
            int pos = 8 + 264 * subs.Count;
            for (int i = 0; i < subs.Count; i++) { o.Bytes(subs[i].name).U32((uint)pos); pos += bodies[i].Length; }
            foreach (byte[] bd in bodies) o.Bytes(bd);
            return o.ToArray();
        }

        /// only_flags_differ(a, b): strings, pool, nodes and attr name / value / type equal.
        public static bool OnlyFlagsDiffer(byte[] a, byte[] b)
        {
            UxBxml A = new UxBxml(a), B = new UxBxml(b);
            if (A.strings.Count != B.strings.Count || A.nodes.Count != B.nodes.Count || A.attrs.Count != B.attrs.Count || !UxBin.Same(A.pool, B.pool)) return false;
            for (int i = 0; i < A.strings.Count; i++) if (A.strings[i] != B.strings[i]) return false;
            for (int i = 0; i < A.nodes.Count; i++) for (int k = 0; k < 8; k++) if (A.nodes[i][k] != B.nodes[i][k]) return false;
            for (int i = 0; i < A.attrs.Count; i++) if (A.attrs[i][0] != B.attrs[i][0] || A.attrs[i][1] != B.attrs[i][1] || A.attrs[i][3] != B.attrs[i][3]) return false;
            return true;
        }
        public static bool SceneOnlyFlagsDiffer(byte[] a, byte[] b)
        {
            var A = SceneChunks(a, out uint ma); var B = SceneChunks(b, out uint mb);
            if (ma != mb || A.Count != B.Count) return false;
            for (int i = 0; i < A.Count; i++)
            {
                if (!UxBin.Same(A[i].name, B[i].name) || A[i].nch != B[i].nch || A[i].nent != B[i].nent) return false;
                for (int k = 0; k < A[i].chunks.Count; k++) if (!OnlyFlagsDiffer(A[i].chunks[k], B[i].chunks[k])) return false;
            }
            return true;
        }

        // ------------------------------------------------------------------------------------------- checks
        /// openreflex_rules(folder): the problems under Open Reflex's readable rules and a re-read of the set (empty = OK).
        public static List<string> Rules(string folder)
        {
            var probs = new List<string>();
            var sets = new SortedSet<string>(StringComparer.Ordinal);
            foreach (string f in Directory.GetFiles(folder, "*.dx9.*")) sets.Add(Path.GetFileName(f).Split('.')[0]);
            if (sets.Count != 1) { probs.Add("expected one slot set, found " + sets.Count); return probs; }
            string slot = sets.Min;
            var missing = new List<string>();
            foreach (string k in UxCommon.SlotFileKinds) if (!File.Exists(Path.Combine(folder, UxCommon.SlotFileName(slot, k)))) missing.Add(k);
            if (missing.Count > 0) { probs.Add("incomplete set, missing " + string.Join(", ", missing)); return probs; }
            string P(string k) => Path.Combine(folder, UxCommon.SlotFileName(slot, k));
            if (!NAME_RX.IsMatch(slot)) probs.Add(slot + " cannot be a level name");
            if (!KIND_RX.IsMatch(slot)) probs.Add(slot + " does not match the Open Reflex track regex");
            long plen = new FileInfo(P("package")).Length;
            try
            {
                var ts = new UxTrackSet(slot, File.ReadAllBytes(P("database")), a => null);
                if (ts.packages.Count == 0) probs.Add(".dx9.database lists no package");
                long end = 0; foreach (var a in ts.assets) end = Math.Max(end, a.AbsOffset + a.Size);
                if (end != plen) probs.Add("database does not cover the package (" + end + " of " + plen + " bytes)");
            }
            catch (Exception ex) { probs.Add("database/package do not read: " + ex.Message); }
            if (plen == 0) probs.Add(".dx9.package is empty");
            var names = new List<string>();
            try
            {
                foreach (var s in UxPcScene.Load(File.ReadAllBytes(P("scene")))) names.Add(s.name);
                if (!names.Contains(slot + "Base")) probs.Add(".dx9.scene has no Base section");
            }
            catch (Exception ex) { probs.Add("scene does not read: " + ex.Message); }
            try
            {
                var lv = new UxBxml(File.ReadAllBytes(P("level")));
                for (int i = 0; i < lv.nodes.Count; i++)
                    if (lv.Name(i) == "SubScene" || lv.Name(i) == "Subscene")
                    {
                        string s = lv.AttrStr(i, "name");
                        if (!names.Contains(s)) probs.Add("level lists a sub-scene missing from the scene: " + s);
                    }
            }
            catch (Exception ex) { probs.Add("level does not read: " + ex.Message); }
            return probs;
        }

        // ------------------------------------------------------------------------------------------- mod.ini, preview, sha256
        /// The menu name: '<display name> (Untamed)'.
        public static string MenuName(UxLevelRow lv) => lv.display + " (Untamed)";
        /// mod.ini (UTF-8, '\n' line ends). date: 'YYYY-MM-DD' as the reference writes it, or null (no date).
        /// A freestyle arena (kind 'freeride', Beta_Fmx slot) adds the reference's free-ride note to the description.
        public static string ModIni(UxLevelRow lv, string author = AUTHOR, string date = null, string kind = "supercross")
        {
            string desc = lv.display + CONVERTED + (UxCommon.IsFmx(lv) ? "; free ride: the arena's ramps and dirt jumps, no race" : "") + (date != null ? "; " + date : "") + ".";
            return "name = " + MenuName(lv) + "\nauthor = " + author + "\ndescription = " + desc + "\nthumbnail = " + PREVIEW + "\nkind = " + kind + "\n";
        }

        /// preview.png: an 8-bit RGB picture scaled to 512 px wide (LANCZOS, as ux_package scales its render), written as Pillow does.
        public static byte[] Preview(byte[] png)
        {
            var im = UxImaging.ReadPngRgb(png);
            int h = Math.Max(1, (int)UxNp.Round((double)im.h * PREVIEW_WIDTH / im.w));
            if (im.w != PREVIEW_WIDTH || im.h != h) im = UxPil.Resize(im, PREVIEW_WIDTH, h, UxPil.Filter.Lanczos);
            return UxImaging.PngRgb(im.data, im.w, im.h);
        }

        /// sha256.txt of a folder: '<sha256> *<name>' for every other file, sorted as the reference sorts them (ignoring case).
        public static string Sha256List(string folder, out List<(string name, long bytes, string sha256)> files)
        {
            var names = new List<string>();
            foreach (string f in Directory.GetFiles(folder)) if (Path.GetFileName(f) != SHA256) names.Add(Path.GetFileName(f));
            names.Sort((a, b) => string.CompareOrdinal(UxCommon.PyLower(a), UxCommon.PyLower(b)));
            var sb = new StringBuilder(); files = new List<(string, long, string)>();
            foreach (string n in names)
            {
                string p = Path.Combine(folder, n), h = UxBin.Sha256File(p);
                sb.Append(h).Append(" *").Append(n).Append('\n');
                files.Add((n, new FileInfo(p).Length, h));
            }
            return sb.ToString();
        }

        /// assemble: writes the release set into `folder`. The package is already there (written by the builder, a byte copy by
        /// nature); database and scene get the stock flags, the level is written as given. picture: an 8-bit RGB PNG for preview.png.
        /// Returns (name, bytes, sha256) of every file listed in sha256.txt.
        public static List<(string name, long bytes, string sha256)> Write(string folder, string slot, UxLevelRow lv, byte[] database, byte[] level, byte[] scene, byte[] picture,
            string author = AUTHOR, string date = null, bool check = true, string kind = "supercross")
        {
            // ---- stock attr flags
            if (!UxBin.Same(PatchDoc(database, out _, false), database) || !UxBin.Same(SceneRebuild(scene, out _, false), scene))
                throw new UxException("unpatched re-encode is not byte-identical, so a flag patch could change more than flags");
            byte[] ndb = PatchDoc(database, out _), nsc = SceneRebuild(scene, out _);
            if (!OnlyFlagsDiffer(database, ndb)) throw new UxException("database: more than flags changed");
            if (!SceneOnlyFlagsDiffer(scene, nsc)) throw new UxException("scene: more than flags changed");
            void Put(string name, byte[] data)
            {
                string p = Path.Combine(folder, name), tmp = p + ".tmp";
                File.WriteAllBytes(tmp, data);
                if (File.Exists(p)) File.Delete(p);
                File.Move(tmp, p);
            }
            Put(UxCommon.SlotFileName(slot, "database"), ndb);
            Put(UxCommon.SlotFileName(slot, "scene"), nsc);
            Put(UxCommon.SlotFileName(slot, "level"), level);
            foreach (string stale in new[] { MOD_INI, PREVIEW, SHA256 }) if (File.Exists(Path.Combine(folder, stale))) File.Delete(Path.Combine(folder, stale));
            // ---- re-read and rule checks on the written files
            if (check)
            {
                var probs = Rules(folder);
                foreach (var a in new UxBxml(ndb).attrs) if (Numeric(a[3]) && a[2] == 0) { probs.Add("numeric database attrs still have flag 0"); break; }
                if (probs.Count > 0) throw new UxException("output " + folder + " breaks its checks: " + string.Join("; ", probs));
            }
            // ---- preview.png, mod.ini, sha256.txt
            Put(PREVIEW, Preview(picture));
            Put(MOD_INI, new UTF8Encoding(false).GetBytes(ModIni(lv, author, date, kind)));
            string list = Sha256List(folder, out var files);
            Put(SHA256, new UTF8Encoding(false).GetBytes(list));
            return files;
        }
    }
}

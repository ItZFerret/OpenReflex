// Path logic of the install / import / mods code, pure (System only: no UnityEngine, no file system) so tests/paths compiles it with
// Unity's bundled csc and feeds it Windows-style paths on any machine. It never assumes a drive letter: a path is a drive path
// ("D:\x", any letter A-Z), a UNC path ("\\server\share\x", also a mapped network drive's target), or a rooted POSIX path.
//   Clean      what a player types or pastes into a folder field -> one normal path (quotes, file:// URLs, \\?\ prefixes, %VARS%, ~,
//              a bare drive "E:", '/' for '\', doubled and trailing separators)
//   Validate   a clear sentence for a path that cannot be a folder (illegal characters, no drive, "D:foo", device names)
//   Parent / LabelOf / SamePath / IsUnder / RelativeId   the same operations as System.IO.Path, correct for both styles on every host
//   RemapDrive the same folder on the other drives (a USB drive that came back as another letter)
//   FirstPathLine   the path in reflex-game-dir.txt (BOM, comments, quotes, several lines)
//   Steam          libraryfolders.vdf libraries
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content
{
    public static class ReflexPaths
    {
        public static bool IsSep(char c) => c == '/' || c == '\\';
        static bool IsLetter(char c) => (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');

        /// "D:" at the start (a drive path, or the current folder of that drive when nothing follows).
        public static bool HasDrive(string p) => p != null && p.Length >= 2 && IsLetter(p[0]) && p[1] == ':';
        /// "D:\" or "D:/" at the start: a drive path with its root.
        public static bool IsDriveRooted(string p) => HasDrive(p) && p.Length >= 3 && IsSep(p[2]);
        /// "\\server\share" (either separator), the form of a network location and of a mapped drive's target.
        public static bool IsUnc(string p) => p != null && p.Length >= 3 && IsSep(p[0]) && IsSep(p[1]) && !IsSep(p[2]);
        /// The path names a place from the root: a drive path, a UNC path or a POSIX path.
        public static bool IsRooted(string p) => IsDriveRooted(p) || IsUnc(p) || (!string.IsNullOrEmpty(p) && p[0] == '/' && !(p.Length > 1 && p[1] == '/'));
        /// The upper-case drive letter of a drive path ("d:\x" -> 'D'), '\0' for the others.
        public static char DriveLetter(string p) => HasDrive(p) ? char.ToUpperInvariant(p[0]) : '\0';

        /// Length of the root of p: "D:\" is 3 ("D:" 2), "\\server\share" with its separator, "/" 1; 0 for a relative path.
        public static int RootLength(string p)
        {
            if (string.IsNullOrEmpty(p)) return 0;
            if (HasDrive(p)) return p.Length >= 3 && IsSep(p[2]) ? 3 : 2;
            if (IsUnc(p))
            {
                int i = 2, parts = 0;
                while (i < p.Length && parts < 2)
                {
                    while (i < p.Length && !IsSep(p[i])) i++;
                    parts++;
                    if (parts < 2) { if (i < p.Length) i++; else break; }
                }
                return i < p.Length ? i + 1 : i;
            }
            return IsSep(p[0]) ? 1 : 0;
        }

        // ------------------------------------------------------------------------------------------------ typed paths

        /// A typed or pasted path as one normal path. windowsHost: the machine is Windows (a '/' becomes '\'; a path with a drive letter or a
        /// UNC name is Windows-style anyway). home / env: the home folder for "~" and the lookup for %NAME% (null: none). "" for null / blank.
        public static string Clean(string value, bool windowsHost, string home = null, Func<string, string> env = null)
        {
            if (value == null) return "";
            string s = StripQuotes(value);
            if (s.Length == 0) return "";
            if (s.StartsWith("file:", StringComparison.OrdinalIgnoreCase)) s = FromFileUrl(s);
            s = StripExtendedPrefix(s);
            if (env != null && s.IndexOf('%') >= 0) s = Regex.Replace(s, "%([^%\\\\/:]+)%", m => { string v = env(m.Groups[1].Value); return string.IsNullOrEmpty(v) ? m.Value : v; });
            if (!string.IsNullOrEmpty(home) && (s == "~" || s.StartsWith("~/", StringComparison.Ordinal) || s.StartsWith("~\\", StringComparison.Ordinal))) s = home + s.Substring(1);
            if (s.Length == 2 && HasDrive(s)) s += "\\";                  // "E:" alone means that drive's root here
            bool windows = windowsHost || HasDrive(s) || IsUnc(s);
            if (windows) s = s.Replace('/', '\\');
            // doubled separators (but not the two of a UNC name)
            int keep = IsUnc(s) ? 2 : 0;
            if (s.Length > keep + 1)
            {
                var b = new StringBuilder(s.Length); b.Append(s, 0, keep);
                for (int i = keep; i < s.Length; i++) if (!(IsSep(s[i]) && i > 0 && IsSep(s[i - 1]))) b.Append(s[i]);
                s = b.ToString();
            }
            int root = RootLength(s), end = s.Length;
            if (IsUnc(s) && root > 0 && IsSep(s[root - 1])) root--;          // (the separator after a share name is not part of the name)
            while (end > root && end > 1 && IsSep(s[end - 1])) end--;
            s = s.Substring(0, end);
            return s;
        }

        static string StripQuotes(string value)
        {
            string s = value.Trim();
            while (s.Length >= 2)
            {
                char a = s[0], z = s[s.Length - 1];
                bool quoted = (a == '"' && z == '"') || (a == '\'' && z == '\'') || (a == '\u201C' && z == '\u201D') || (a == '\u2018' && z == '\u2019');
                if (!quoted) break;
                s = s.Substring(1, s.Length - 2).Trim();
            }
            // a lone quote left from a copy that cut it off
            return s.Trim('"', '\'', '\u201C', '\u201D', '\u2018', '\u2019').Trim();
        }

        /// file:///D:/x, file:/D:/x, file://localhost/D:/x, file://server/share/x (UNC), %20 escapes.
        static string FromFileUrl(string s)
        {
            string rest = s.Substring(5);
            int slashes = 0; while (slashes < rest.Length && rest[slashes] == '/') slashes++;
            string host = "", path;
            if (slashes == 2)
            {
                int end = rest.IndexOf('/', 2);
                host = end < 0 ? rest.Substring(2) : rest.Substring(2, end - 2);
                path = end < 0 ? "" : rest.Substring(end);
            }
            else path = "/" + rest.Substring(slashes);
            try { path = Uri.UnescapeDataString(path); host = Uri.UnescapeDataString(host); } catch (UriFormatException) { }
            if (host.Length > 0 && !host.Equals("localhost", StringComparison.OrdinalIgnoreCase)) return "\\\\" + host + path.Replace('/', '\\');
            if (path.Length > 2 && path[0] == '/' && HasDrive(path.Substring(1))) path = path.Substring(1);   // "/D:/x"
            return path;
        }

        /// \\?\D:\x -> D:\x, \\?\UNC\server\share -> \\server\share (also \\.\ and forward slashes).
        static string StripExtendedPrefix(string s)
        {
            if (s.Length >= 4 && IsSep(s[0]) && IsSep(s[1]) && (s[2] == '?' || s[2] == '.') && IsSep(s[3]))
            {
                string r = s.Substring(4);
                if (r.Length >= 4 && r.StartsWith("UNC", StringComparison.OrdinalIgnoreCase) && IsSep(r[3])) return "\\\\" + r.Substring(4);
                return r;
            }
            return s;
        }

        static readonly char[] WindowsBad = { '<', '>', '"', '|', '?', '*' };
        static readonly string[] Devices = { "CON", "PRN", "AUX", "NUL" };
        static bool IsDeviceName(string seg)
        {
            int dot = seg.IndexOf('.'); string b = (dot < 0 ? seg : seg.Substring(0, dot)).TrimEnd(' ').ToUpperInvariant();
            if (Array.IndexOf(Devices, b) >= 0) return true;
            return b.Length == 4 && (b.StartsWith("COM", StringComparison.Ordinal) || b.StartsWith("LPT", StringComparison.Ordinal)) && b[3] >= '1' && b[3] <= '9';
        }

        /// A short folder to suggest on the drive of path ("D:\Games\OpenReflex" for a path on D:, the share for a network path).
        public static string ShortFolderExample(string path)
        {
            if (HasDrive(path)) return char.ToUpperInvariant(path[0]) + ":\\Games\\OpenReflex";
            if (IsUnc(path)) return path.Substring(0, RootLength(path)).TrimEnd('/', '\\') + "\\Games\\OpenReflex";
            return "C:\\Games\\OpenReflex";
        }

        /// A sentence saying why a cleaned path cannot be a folder, null when it can (a folder that does not exist is not checked here).
        public static string Validate(string cleaned, bool windowsHost)
        {
            if (string.IsNullOrEmpty(cleaned)) return "Type or paste the path of a folder.";
            foreach (char c in cleaned) if (c < ' ') return "That path contains a character a folder name cannot have.";
            bool windows = windowsHost || HasDrive(cleaned) || IsUnc(cleaned);
            if (windows)
            {
                if (HasDrive(cleaned) && !IsDriveRooted(cleaned))
                    return "Type the full path with a backslash after the drive, for example " + cleaned.Substring(0, 2) + "\\Games\\MX vs ATV Reflex.";
                if (!IsRooted(cleaned))
                    return "Type the full path, starting with the drive letter, for example D:\\Games\\MX vs ATV Reflex (or a network path such as \\\\server\\share\\Games).";
                string body = cleaned.Substring(HasDrive(cleaned) ? 2 : 0);
                if (body.IndexOfAny(WindowsBad) >= 0) return "A folder path cannot contain any of < > \" | ? *";
                if (body.IndexOf(':') >= 0) return "A folder path can only have a colon after the drive letter.";
                foreach (string seg in cleaned.Substring(RootLength(cleaned)).Split('\\', '/'))
                {
                    if (seg.Length == 0) continue;
                    if (seg.Length > 255) return "A folder name in that path is longer than Windows allows (255 characters).";
                    if (IsDeviceName(seg)) return "\"" + seg + "\" is a reserved Windows name and cannot be a folder.";
                }
            }
            else if (!IsRooted(cleaned)) return "Type the full path of the folder, starting with /.";
            return null;
        }

        /// Why a valid path was not found, given the roots of the ready drives ("D:\", ...): a missing drive, an unreachable network
        /// location, or a missing folder.
        public static string NotFound(string cleaned, IEnumerable<string> readyRoots)
        {
            if (HasDrive(cleaned) && readyRoots != null)
            {
                char want = DriveLetter(cleaned); bool present = false; var have = new List<string>();
                foreach (var r in readyRoots) { if (DriveLetter(r) == want) present = true; if (HasDrive(r)) have.Add(r.Substring(0, 2).ToUpperInvariant()); }
                if (!present) return "Drive " + want + ": is not ready or not connected" + (have.Count > 0 ? " (drives available: " + string.Join(" ", have) + ")" : "") + ".";
            }
            if (IsUnc(cleaned)) return "The network location could not be reached, or the folder is not there: " + cleaned;
            return "Folder not found: " + cleaned;
        }

        // ------------------------------------------------------------------------------------------------ operations

        /// The folder of a path (either separator; a drive root "C:\" or "/" keeps its separator; a UNC share is a root), null at the top.
        public static string Parent(string path)
        {
            if (string.IsNullOrEmpty(path)) return null;
            int root = RootLength(path);
            int end = path.Length;
            while (end > root && end > 1 && IsSep(path[end - 1])) end--;
            if (end <= root) return null;                         // a root has no folder above it
            int i = -1;
            for (int k = end - 1; k >= root; k--) if (IsSep(path[k])) { i = k; break; }
            if (i < 0) return root > 0 ? path.Substring(0, root) : null;
            return path.Substring(0, i);
        }

        /// The last name of a path; for a root (D:\, \\server\share, /) the root itself, so a label is never blank.
        public static string LabelOf(string path)
        {
            if (string.IsNullOrEmpty(path)) return "";
            int root = RootLength(path), end = path.Length;
            while (end > root && end > 1 && IsSep(path[end - 1])) end--;
            if (end <= root)
            {
                if (HasDrive(path)) return char.ToUpperInvariant(path[0]) + ":\\";
                return end > 1 ? path.Substring(0, end).TrimEnd('/', '\\') : path.Substring(0, 1);
            }
            int i = -1; for (int k = end - 1; k >= root; k--) if (IsSep(path[k])) { i = k; break; }
            int start = i < 0 ? root : i + 1;
            return path.Substring(start, end - start);
        }

        /// The same place by name: separators and case (Windows names are case-insensitive) and trailing separators ignored.
        public static bool SamePath(string a, string b)
        {
            if (a == null || b == null) return a == b;
            return string.Equals(Key(a), Key(b), StringComparison.OrdinalIgnoreCase);
        }
        static string Key(string p)
        {
            string s = p.Replace('\\', '/'); int root = RootLength(p);
            int end = s.Length; while (end > root && end > 1 && s[end - 1] == '/') end--;
            return s.Substring(0, end);
        }

        /// child is a folder or file below parent (never the folder itself).
        public static bool IsUnder(string parent, string child)
        {
            if (string.IsNullOrEmpty(parent) || string.IsNullOrEmpty(child)) return false;
            string p = Key(parent), c = Key(child);
            if (!p.EndsWith("/", StringComparison.Ordinal)) p += "/";
            return c.Length > p.Length && c.StartsWith(p, StringComparison.OrdinalIgnoreCase);
        }

        /// dir relative to root with '/' separators ("Honda/CRF 450"), whichever separators the two use; null when dir is not under root.
        public static string RelativeId(string root, string dir)
        {
            if (root == null || dir == null) return null;
            string r = root.Replace('\\', '/').TrimEnd('/'), d = dir.Replace('\\', '/').TrimEnd('/');
            if (!d.StartsWith(r + "/", StringComparison.OrdinalIgnoreCase)) return null;
            string rel = d.Substring(r.Length + 1);
            while (rel.Contains("//")) rel = rel.Replace("//", "/");
            return rel.Length == 0 ? null : rel;
        }

        /// The folder a player's data path sits in: <root>/OpenReflex.app/Contents -> the folder holding the .app (macPlayer), else the parent
        /// of the data folder (Windows: the folder holding the .exe, on any drive or share).
        public static string ClientRootFor(string dataPath, bool macPlayer)
        {
            if (string.IsNullOrEmpty(dataPath)) return null;
            string parent = Parent(dataPath);
            if (macPlayer && parent != null && parent.EndsWith(".app", StringComparison.OrdinalIgnoreCase)) return Parent(parent);
            return parent;
        }

        /// The same path on another drive: "E:\Games\x" with the ready roots {"D:\", "E:\", "F:\"} -> "D:\Games\x", "F:\Games\x" (not E:).
        public static List<string> RemapDrive(string path, IEnumerable<string> readyRoots)
        {
            var o = new List<string>();
            if (!IsDriveRooted(path) || readyRoots == null) return o;
            char own = DriveLetter(path);
            foreach (var r in readyRoots)
            {
                if (!HasDrive(r) || DriveLetter(r) == own) continue;
                string x = char.ToUpperInvariant(r[0]) + path.Substring(1);
                if (!o.Contains(x)) o.Add(x);
            }
            return o;
        }

        /// The path in a reflex-game-dir.txt: the first line that is not blank or a comment (# ;), BOM and quotes removed; null if none.
        public static string FirstPathLine(string text, bool windowsHost, string home = null, Func<string, string> env = null)
        {
            if (string.IsNullOrEmpty(text)) return null;
            foreach (string raw in text.Split('\n'))
            {
                string line = raw.Trim('\uFEFF', '\r', ' ', '\t');
                if (line.Length == 0 || line[0] == '#' || line[0] == ';') continue;
                string c = Clean(line, windowsHost, home, env);
                if (c.Length > 0) return c;
            }
            return null;
        }

        // ------------------------------------------------------------------------------------------------ reparse points

        public const uint ReparseSymlink = 0xA000000C, ReparseMountPoint = 0xA0000003;
        /// Windows: a folder is a link to elsewhere (a symbolic link, or a junction) by its reparse tag. A volume mounted into a folder (tag
        /// mount point, but the folder is a volume mount point) is a real place of that drive, and cloud placeholders (OneDrive) and the like
        /// are real folders too.
        public static bool IsLinkTag(uint tag, bool volumeMountPoint) => tag == ReparseSymlink || (tag == ReparseMountPoint && !volumeMountPoint);

        // ------------------------------------------------------------------------------------------------ Steam

        /// The library folders of a Steam libraryfolders.vdf (new format: "path" entries; old: "1" "D:\\SteamLibrary"), unescaped. A Linux /
        /// macOS library ("/run/media/mmcblk0p1", "/home/deck/.local/share/Steam") is given through unixDrive when one is named (Proton: "Z:"
        /// is the Linux root, so "/run/media/x" is "Z:\run\media\x"), else as the POSIX path itself.
        public static List<string> SteamLibraries(string vdf, string unixDrive = null)
        {
            var o = new List<string>();
            if (string.IsNullOrEmpty(vdf)) return o;
            foreach (Match m in Regex.Matches(vdf, "(?m)^\\s*\"(path|[0-9]+)\"\\s+\"((?:[^\"\\\\]|\\\\.)*)\""))
            {
                string v = m.Groups[2].Value.Replace("\\\\", "\\");
                string c = v.StartsWith("/", StringComparison.Ordinal) && !v.StartsWith("//", StringComparison.Ordinal)
                    ? (unixDrive != null ? UnixToWine(v, unixDrive) : Clean(v, false)) : Clean(v, true);
                if (IsRooted(c) && !o.Contains(c)) o.Add(c);
            }
            return o;
        }

        /// A Linux path as Wine / Proton shows it: "/home/deck/x" -> "Z:\home\deck\x" (drive: the drive mapped to "/", Z: by default).
        public static string UnixToWine(string path, string drive = "Z:")
        {
            if (string.IsNullOrEmpty(path) || path[0] != '/') return path;
            string d = drive.TrimEnd('\\', '/');
            string rest = path.Replace('/', '\\').TrimEnd('\\');
            while (rest.Contains("\\\\")) rest = rest.Replace("\\\\", "\\");
            return rest.Length == 0 ? d + "\\" : d + rest;
        }

        static readonly Regex GameName = new Regex(@"^\s*mx\s*vs\.?\s*atv\s*reflex", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        /// A folder name that is the Reflex game's ("MX vs. ATV Reflex", "MX vs ATV Reflex (Steam)", "mx vs atv reflex").
        public static bool IsReflexFolderName(string name) => name != null && GameName.IsMatch(name);
    }
}

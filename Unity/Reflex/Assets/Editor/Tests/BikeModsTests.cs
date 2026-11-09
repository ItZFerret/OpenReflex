using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // The bike mods' folder rules (Content/ReflexMods.cs, Content/BikeMods.cs): the client root of a macOS / Windows / Linux player, Windows
    // and mixed separators, junk names, mod.ini, the recursive scan of a scratch Mods/Bikes tree (nested user folders, spaces, Unicode,
    // upper-case files, AppleDouble / __MACOSX / .DS_Store / Thumbs.db junk, a database without its package, two mods of one name), the
    // namespaces. With REFLEX_BIKEMOD_ROOT (a Mods/Bikes folder holding the user's "New Bike Models"): the real mod's packages are read.
    // Run: Unity -batchmode -projectPath <copy> -executeMethod Reflex.Port.Editor.BikeModsTests.Verify -quit
    public static class BikeModsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[BIKEMODS] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static void Eq(string got, string want, string what) => Check(got == want, what + ": \"" + got + "\"" + (got == want ? "" : " (want \"" + want + "\")"));

        static void Touch(string path, int bytes = 16) { Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllBytes(path, new byte[bytes]); }

        public static void Verify()
        {
            report.Clear(); bad = 0;
            string tmp = null;
            try
            {
                // 1. the client root
                Eq(ReflexMods.ClientRootFor("/Users/ann/Games/Open Reflex/OpenReflex.app/Contents", true), "/Users/ann/Games/Open Reflex", "macOS player: the folder holding the .app");
                Eq(ReflexMods.ClientRootFor(@"C:\Games\Open Reflex\OpenReflex_Data", false), @"C:\Games\Open Reflex", "Windows player: the folder holding OpenReflex.exe");
                Eq(ReflexMods.ClientRootFor(@"D:\OpenReflex_Data\", false), @"D:\", "Windows player at a drive root");
                Eq(ReflexMods.ClientRootFor(@"\\server\share\Reflex\OpenReflex_Data", false), @"\\server\share\Reflex", "Windows UNC path");
                Eq(ReflexMods.ClientRootFor("/home/ann/reflex/OpenReflex_Data", false), "/home/ann/reflex", "Linux player");
                Eq(ReflexMods.ClientRootFor("/Users/ann/dev/REFLEX/Unity/Reflex/Assets", false), "/Users/ann/dev/REFLEX/Unity/Reflex", "editor: the project folder");
                Eq(ReflexMods.ClientRootFor(@"C:\Users\Zoë\Bikes Game\OpenReflex_Data", false), @"C:\Users\Zoë\Bikes Game", "Windows path with spaces and Unicode");

                // 2. relative ids with either separator
                Eq(ReflexMods.RelativeId(@"C:\Games\Mods\Bikes", @"C:\Games\Mods\Bikes\My Stuff\New Bike Models"), "My Stuff/New Bike Models", "Windows id");
                Eq(ReflexMods.RelativeId(@"C:\Games\Mods\Bikes\", @"c:/games/mods/bikes/Ünïcode Bikes\2010 pack"), "Ünïcode Bikes/2010 pack", "mixed separators and case");
                Eq(ReflexMods.RelativeId("/a/Mods/Bikes", "/a/Mods/Bikes2/x"), null, "a sibling folder is not under the root");
                Eq(ReflexMods.RelativeId("/a/Mods/Bikes", "/a/Mods/Bikes"), null, "the root itself has no id");

                // 3. junk
                foreach (var j in new[] { ".DS_Store", "._MXs.dx9.database", "Thumbs.db", "THUMBS.DB", "desktop.ini", "__MACOSX", "$RECYCLE.BIN", "" }) Check(ReflexMods.IsJunk(j), "junk: " + j);
                foreach (var j in new[] { "MXs.dx9.database", "New Bike Models", "mod.ini", "Thumbs" }) Check(!ReflexMods.IsJunk(j), "not junk: " + j);

                // 4. mod.ini
                var ini = BikeMods.ParseIni("\uFEFF; comment\r\n[Mod]\r\nName = \"Factory Bikes\"\r\n# other\r\nAUTHOR=Zoë\r\ndescription = Two = signs\r\nnonsense\r\n");
                Eq(ini.TryGetValue("name", out var n) ? n : null, "Factory Bikes", "ini name (BOM, CRLF, quotes, section)");
                Eq(ini.TryGetValue("author", out var au) ? au : null, "Zoë", "ini key case");
                Eq(ini.TryGetValue("description", out var de) ? de : null, "Two = signs", "ini value with '='");

                // 5. namespaces
                var (id, db) = BikeMods.Split("mod:My Stuff/New Bike Models>HR_MXs");
                Eq(id, "My Stuff/New Bike Models", "namespace id"); Eq(db, "HR_MXs", "namespace database");
                Eq(BikeMods.BaseDb("MXs"), "MXs", "an original database"); Eq(BikeMods.NamespaceOf("MXs"), "", "an original database has no namespace");

                // 6. the scan of a scratch tree
                tmp = Path.Combine(Path.GetTempPath(), "bikemods-test-" + Guid.NewGuid().ToString("N"));
                string bikes = Path.Combine(tmp, "Mods", "Bikes");
                string a = Path.Combine(bikes, "My Folder", "Sub folder", "New Bike Models ü");
                Touch(Path.Combine(a, "MXs.dx9.database")); Touch(Path.Combine(a, "MXs.dx9.package"));
                Touch(Path.Combine(a, "HR_MXS.DX9.DATABASE")); Touch(Path.Combine(a, "hr_mxs.dx9.package"));
                Touch(Path.Combine(a, "._MXs.dx9.database")); Touch(Path.Combine(a, ".DS_Store")); Touch(Path.Combine(a, "Thumbs.db"));
                Touch(Path.Combine(a, "Nested", "MX_GraphicKits.dx9.database")); Touch(Path.Combine(a, "Nested", "MX_GraphicKits.dx9.package"));   // inside a mod: part of it, not a mod
                string b = Path.Combine(bikes, "Kits only");
                Touch(Path.Combine(b, "MX_GraphicKits.dx9.database")); Touch(Path.Combine(b, "MX_GraphicKits.dx9.package"));
                File.WriteAllText(Path.Combine(b, "mod.ini"), "name = Kit Pack\nauthor = Someone\nthumbnail = missing.png\n");
                string c = Path.Combine(bikes, "Other", "New Bike Models ü");   // the same folder name as a
                Touch(Path.Combine(c, "MXs.dx9.database")); Touch(Path.Combine(c, "MXs.dx9.package"));
                File.WriteAllText(Path.Combine(c, "mod.json"), "{\"description\": \"json info\"}");
                Touch(Path.Combine(bikes, "__MACOSX", "New Bike Models", "MXs.dx9.database")); Touch(Path.Combine(bikes, "__MACOSX", "New Bike Models", "MXs.dx9.package"));
                Touch(Path.Combine(bikes, "Broken", "MXs.dx9.database"));   // no package: not a mod
                Touch(Path.Combine(bikes, "MXs.dx9.database")); Touch(Path.Combine(bikes, "MXs.dx9.package"));   // loose in Bikes: not a mod
                string deep = bikes; for (int i = 0; i < 9; i++) deep = Path.Combine(deep, "level " + i);
                Touch(Path.Combine(deep, "MXs.dx9.database")); Touch(Path.Combine(deep, "MXs.dx9.package"));
                var mods = BikeMods.Scan(bikes);
                Log("scan: " + string.Join(" | ", mods.Select(m => m.id + " = \"" + m.name + "\" [" + string.Join(",", m.databases) + "]")));
                Check(mods.Count == 4, "four mods (nested user folders, the deep one; not __MACOSX, the loose files, the broken one, the nested folder of a mod): " + mods.Count);
                var ma = mods.FirstOrDefault(m => m.id == "My Folder/Sub folder/New Bike Models ü");
                Check(ma != null, "the nested mod's id has '/' separators and keeps the Unicode name");
                if (ma != null)
                {
                    Check(ma.databases.SequenceEqual(new[] { "HR_MXS", "MXs" }), "its databases, the AppleDouble copy ignored: " + string.Join(",", ma.databases));
                    Eq(ma.name, "New Bike Models ü (My Folder/Sub folder)", "two mods of one folder name: the parent tells them apart");
                }
                var mb = mods.FirstOrDefault(m => m.id == "Kits only");
                Check(mb != null && mb.name == "Kit Pack" && mb.author == "Someone" && mb.thumbnail == null, "mod.ini names it (a missing thumbnail is dropped)");
                var mc = mods.FirstOrDefault(m => m.id == "Other/New Bike Models ü");
                Check(mc != null && mc.description == "json info", "mod.json describes it");
                Check(mods.Any(m => m.id.StartsWith("level 0/level 1/", StringComparison.Ordinal)), "a mod nine folders deep is found");
                Check(BikeMods.Scan(Path.Combine(tmp, "nothing here")).Count == 0, "a missing folder: no mods");

                // 7. the real mod (optional): its packages through the namespace, the chassis model parsed
                string real = Environment.GetEnvironmentVariable("REFLEX_BIKEMOD_ROOT");
                if (!string.IsNullOrEmpty(real) && Directory.Exists(real))
                {
                    var rm = BikeMods.Scan(real).FirstOrDefault(m => m.databases.Any(d => d.Equals("MXs", StringComparison.OrdinalIgnoreCase)));
                    Check(rm != null, "the real mod is found under " + real);
                    if (rm != null)
                    {
                        Log("real mod " + rm.id + ": " + string.Join(",", rm.databases));
                        Check(rm.Has("MXs", "MarsF_Lite") && rm.ChassisModel("MXs", "MarsF_Lite", "MX_MarsF_Lite") == "MX_MarsF_Lite", "MXs/MarsF_Lite and its chassis MX_MarsF_Lite");
                        Check(rm.Has("MX_GraphicKits", "MX_MarsF_Lite_THQ_Red") && rm.Has("HR_MXs", "HR_MarsF_Lite"), "a graphic kit and the HR twin");
                        Check(!rm.Has("MX_Tires", "MX_Tire_THQ"), "no tyres in the mod (the originals stay)");
                        string d = BikeMods.Describe(rm, "MXs", "MarsF_Lite", "MX_MarsF_Lite");
                        Log("real chassis: " + d);
                        Check(d != null && d.Contains("MX_Plstc") && d.Contains("database mod:" + rm.id + ">MXs mod=True"), "the mod chassis parses through the namespace and binds the kit's MX_Plstc");
                        string hr = BikeMods.Describe(rm, "HR_MXs", "HR_MarsF_Lite", "HR_MX_MarsF_Lite");
                        Log("real HR chassis: " + hr);
                        Check(hr != null, "the HR chassis parses");
                        // every chassis (and its HR twin): each skinned surface follows the nodes its vertices are weighted to (a 30 degree
                        // steer through OriginalBikeSkin), whatever node the surface is bound to (VenusF's rear shock is bound to OBJ_UPR_Forks)
                        foreach (string family in new[] { "Earth", "Mars", "Mercury", "Neptune", "Venus" })
                            foreach (string pkg in new[] { family, family + "_Lite", family + "F", family + "F_Lite" })
                                foreach (var (cdb, p) in new[] { ("MXs", pkg), ("HR_MXs", "HR_" + pkg) })
                                {
                                    string model = rm.ChassisModel(cdb, p, (cdb == "HR_MXs" ? "HR_MX_" : "MX_") + pkg);
                                    if (model == null) continue;
                                    var lines = BikeMods.SkinCheck(rm, cdb, p, model);
                                    foreach (var l in lines.Where(l => l.Contains("SKINNED"))) Log("  " + cdb + "/" + model + " " + l);
                                    Check(lines.Last() == "0 bad", cdb + "/" + model + ": every skinned surface follows its weights under a steer");
                                }
                    }
                }
                else Log("(REFLEX_BIKEMOD_ROOT not set: the real mod is not read)");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally { try { if (tmp != null && Directory.Exists(tmp)) Directory.Delete(tmp, true); } catch (Exception) { } }
            Log(bad == 0 ? "PASS" : "FAIL " + bad);
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}

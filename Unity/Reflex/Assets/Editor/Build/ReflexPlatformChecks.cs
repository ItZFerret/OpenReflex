// Batch checks of the platform layer (runtests.sh style: -executeMethod Reflex.Port.EditorTools.ReflexPlatformChecks.<Method>;
// each logs "verify: PASS" or "verify: FAIL ..." and exits 0 / 1).
//   Plugin   the loaded libreflexworld: API version and every struct size against the managed layouts (ReflexWorldNative.Check)
//   Report   ReflexSupport.CreateReport into a temp folder: the ZIP opens, has system-info.txt, and holds no home folder path
//   Hooks    the options pages' ReflexOptionsHooks.CreateSupportReport / OpenLogFolder are set by ReflexSupport (PASS "skipped" when
//            the options code is not in the project)
using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Platform;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.EditorTools
{
    public static class ReflexPlatformChecks
    {
        static void Verdict(bool ok, string what)
        {
            Debug.Log("[Reflex] " + what);
            Debug.Log(ok ? "verify: PASS" : "verify: FAIL " + what);
            if (Application.isBatchMode) EditorApplication.Exit(ok ? 0 : 1);
        }

        public static void Plugin()
        {
            var c = ReflexWorldNative.Check();
            foreach (var (name, size) in ReflexWorldNative.ManagedSizes()) Debug.Log($"[Reflex] struct {name}: managed {size}");
            Verdict(c.Loaded && c.Versioned && c.Compatible, "plugin check: " + c.Summary);
        }

        public static void Hooks()
        {
            Type hooks = null;
            foreach (var a in AppDomain.CurrentDomain.GetAssemblies()) if ((hooks = a.GetType("Reflex.Port.Options.ReflexOptionsHooks", false)) != null) break;
            if (hooks == null) { Verdict(true, "hooks: skipped (no Reflex.Port.Options.ReflexOptionsHooks in this project)"); return; }
            typeof(ReflexSupport).GetMethod("HookOptions", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static).Invoke(null, null);
            var create = hooks.GetField("CreateSupportReport").GetValue(null) as Func<string>;
            var open = hooks.GetField("OpenLogFolder").GetValue(null) as Action;
            string zip = null;
            if (create != null)
            {
                zip = create();
                try { File.Delete(zip); } catch (Exception) { }
            }
            Verdict(create != null && open != null && zip != null && zip.EndsWith(".zip"), $"hooks: CreateSupportReport {(create != null ? "set" : "null")} (made {zip}), OpenLogFolder {(open != null ? "set" : "null")}");
        }

        public static void Report()
        {
            string dir = Path.Combine(Path.GetTempPath(), "reflex-report-check-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            try
            {
                string zip = ReflexSupport.CreateReport(dir);
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                using var z = ZipFile.OpenRead(zip);
                var names = z.Entries.Select(e => e.FullName).ToList();
                bool leak = false;
                foreach (var e in z.Entries)
                {
                    using var r = new StreamReader(e.Open());
                    if (r.ReadToEnd().Contains(home)) { leak = true; Debug.LogWarning("[Reflex] home folder in " + e.FullName); }
                }
                Verdict(names.Contains("system-info.txt") && !leak, $"report {Path.GetFileName(zip)}: {names.Count} entries ({string.Join(", ", names)}){(leak ? ", HOME FOLDER LEAKED" : "")}");
            }
            catch (Exception e) { Verdict(false, "report: " + e); }
            finally { try { Directory.Delete(dir, true); } catch (Exception) { } }
        }
    }
}

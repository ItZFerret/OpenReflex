// TEST ONLY: the location art of the install, measured (page "art-survey"), and the hub / loading backdrops as a player sees them.
using System.Collections;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseArt
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("art-survey", Survey);
            UiShowcase.Register("loading", Loading);
        }

        static IEnumerator Survey()
        {
            var sb = new StringBuilder();
            var t = FeTables.Shared;
            foreach (var r in t.Rows("Icon"))
            {
                int id = FeTables.I(r, "IconId");
                if (!t.IconAsset(id, out string asset, out string package, out string database)) continue;
                var a = ReflexInstall.Packages.Find(database, package, asset, "texture");
                string size = "-";
                if (a != null) { try { var d = ReflexInstall.Packages.Read(a); if (ReflexUiData.TextureHeader(d, out int w, out int h, out string f, out int lv, out _)) size = w + "x" + h + " " + f + " mips" + lv; } catch (System.Exception e) { size = "ERR " + e.Message; } }
                else size = "atlas-sprite";
                sb.AppendLine($"icon {id} {database}/{package}/{asset} {size}");
            }
            foreach (var e in OpenContent.Events)
            {
                string S(int i) { if (i < 0) return "-"; var (tex, uv) = OpenContent.Assets.Icon(i); return tex ? $"{tex.width}x{tex.height} uv({uv.width:0.###},{uv.height:0.###})" : "null"; }
                sb.AppendLine($"event {e.key} preview[{e.previewIcon}] {S(e.previewIcon)} poster[{e.posterIcon}] {S(e.posterIcon)} map[{e.mapIcon}] {S(e.mapIcon)} type[{e.typeIcon}] {S(e.typeIcon)}");
            }
            // every large texture asset of every database
            string dbDir = ReflexInstall.DatabaseDir;
            foreach (string f in Directory.GetFiles(dbDir, "*.dx9.database").OrderBy(x => x))
            {
                string name = Path.GetFileName(f).Replace(".dx9.database", "");
                Reflex.Port.Content.Rfx.Database db = null;
                try { db = ReflexInstall.Packages.Db(name); } catch (System.Exception) { }
                if (db == null) continue;
                foreach (var p in db.packages.Values)
                    foreach (var a in p.OfType("texture"))
                    {
                        if (a.size < 300000) continue;
                        try { var d = ReflexInstall.Packages.Read(a); if (ReflexUiData.TextureHeader(d, out int w, out int h, out string fmt, out int lv, out _) && (w >= 1024 || h >= 1024) && w * h >= 1024 * 512) sb.AppendLine($"big {name}/{p.name}/{a.name} {w}x{h} {fmt}"); } catch (System.Exception) { }
                    }
            }
            string path = Path.Combine(System.Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"), "art-survey.txt");
            File.WriteAllText(path, sb.ToString());
            Debug.Log("[ui-showcase] art survey " + path);
            yield return null;
        }

        static IEnumerator Loading()
        {
            var ev = OpenContent.Events.FirstOrDefault(x => x.activity == "National") ?? OpenContent.Events.First();
            var l = OpenLoading.Show(ev, OpenHistory.SetupOf(ev.key));
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("loading");
            Object.Destroy(l.gameObject);
            yield return UiShowcase.Frames(3);
        }
    }
}

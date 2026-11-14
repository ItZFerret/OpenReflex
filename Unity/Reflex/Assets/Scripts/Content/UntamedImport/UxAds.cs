// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_ads.py (DESIGN 23.4): the Untamed MassiveAd
// panels restored (ux_build_track --massive-ads keep, the default of the sx builds; model = as render-only ModelRSData; drop = nothing).
// The level stage classes Untamed MassiveAdEntityRSData 'drop' (so the stage outputs stay as they are); this module converts what a kept
// panel needs, as UxSplitGate does for the split gate:
//   * the panel model (UxMeshes.ConvertModel, render-only, the Untamed draw distance);
//   * its material: UxMaterials.Planner on a level context that also holds the panel's material and texture records (as the level stage
//     would have written them) + UxMaterials.BuildMaterial; textures the texture stage did not convert: UxTextures.ConvertTexture;
//   * the entity: a copy of the stock MassiveAdEntityRSData prototype (keep) or of the ModelRSData prototype (model), id = Untamed id
//     + ID_OFFSET, the Untamed matrix + the level lift, COPY_PROPS, GroupNumber 0, RenderInto*Shadow 0. No collision. Base only.
//   UxAds.Build(ctx, mode, matOpts, sources, templateAssets, lift, groupNumber, paths, log) -> (record, assets, [Entity Elem])
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_ads.py.
    public static class UxAds
    {
        public const string SCHEMA = "ux_ads/1";
        /// ux_entities.COPY_PROPS.
        internal static readonly string[] COPY_PROPS = { "EnableTick", "ExportName", "AlwaysVisible", "CastsShadow", "GroupNumber", "AIAvoid", "LinkedTriggerID",
            "Tier Number", "Is Non Animated", "TriggerType", "TriggerEntityType", "TriggerID", "InverseTickFreq", "LevelConfig" };

        /// What Build returns.
        public sealed class Result { public JObj record; public UxAssetMap assets = new UxAssetMap(); public List<UxElem> elems = new List<UxElem>(); }

        // ---- ux_level.texture_facts / material_facts (same as UxLevel.TextureFacts / MaterialFacts: the level stage's closure records)
        static JObj TextureFacts(byte[] h)
        {
            UxBin.Require(h.Length >= 0x28 + 16, "not an Untamed texture");
            uint magic = UxBin.U32BE(h, 0), nsurf = UxBin.U32BE(h, 12), kind = UxBin.U32BE(h, 20), flags = UxBin.U32BE(h, 24);
            if (magic != UxTex.MAGIC) throw new UxException("not an Untamed texture");
            uint mips = UxBin.U32BE(h, 0x28), w = UxBin.U32BE(h, 0x2C), hh = UxBin.U32BE(h, 0x30), fmt = UxBin.U32BE(h, 0x34);
            return new JObj().Set("width", (long)w).Set("height", (long)hh).Set("mips", (long)mips).Set("frames", (long)nsurf).Set("kind", (long)kind).Set("flags", (long)flags)
                .Set("format", UxTex.Xfmt(fmt).name).Set("d3dformat", fmt.ToString("x8", CultureInfo.InvariantCulture));
        }
        static JObj MaterialFacts(byte[] data)
        {
            var m = UxMat.ReadMaterial(data, true);
            var tex = new List<object>(); var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var p in m.passes)
                foreach (var t in p.textures)
                    if (!string.IsNullOrEmpty(t.Key) && seen.Add(UxCommon.PyLower(t.Key))) tex.Add(t.Key);
            var p0 = m.passes.Count > 0 ? m.passes[0] : null;
            return new JObj().Set("passes", (long)m.passes.Count).Set("shader", p0?.vs).Set("textures", tex).Set("ntextures", (long)tex.Count)
                .Set("blend", p0 != null ? UxMat.BlendClass(p0.states) : null);
        }

        /// _ctx_with(lv, mat_names): a materials planner whose closure also holds the panel materials and their textures.
        static UxMaterials.Planner CtxWith(UxContext ctx, UxMaterialsOptions opts, List<string> matNames, List<object> added)
        {
            var planner = new UxMaterials.Planner(ctx, opts);
            foreach (string m in matNames)
            {
                if (planner.HasMaterial(m)) continue;
                if (!ctx.disc.HasExtract(m, "material")) throw new UxException("panel material " + m + " not in extract/");
                byte[] mb = ctx.disc.ReadExtract(m, "material", ctx.cancel);
                var f = MaterialFacts(mb);
                var rec = new JObj(); foreach (var kv in f.items) rec.Set(kv.Key, kv.Value);
                rec.Set("file", "material/" + m + ".material").Set("name", m).Set("entities", new List<object>()).Set("terrain", false).Set("md5", UxBin.Md5(mb))
                    .Set("size", (long)mb.Length).Set("package", "Shared").Set("ref_names", new List<object> { m });
                planner.AddMaterial(m, rec); added.Add(m + ".material");
                foreach (object to in UxJson.Arr(f.Get("textures")))
                {
                    string t = (string)to;
                    if (planner.HasTexture(t)) continue;
                    if (!ctx.disc.HasExtract(t, "texture")) throw new UxException("panel texture " + t + " not in extract/");
                    byte[] tb = ctx.disc.ReadExtract(t, "texture", ctx.cancel);
                    var tr = new JObj(); foreach (var kv in TextureFacts(tb).items) tr.Set(kv.Key, kv.Value);
                    tr.Set("file", "texture/" + t + ".texture").Set("name", t).Set("entities", new List<object>()).Set("terrain", false).Set("materials", new List<object> { m })
                        .Set("md5", UxBin.Md5(tb)).Set("size", (long)tb.Length).Set("package", "Shared").Set("ref_names", new List<object> { t });
                    planner.AddTexture(t, tr); added.Add(t + ".texture");
                }
            }
            return planner;
        }

        static bool Truthy(object v) => v is string s ? s.Length > 0 : v is bool b ? b : v is long l ? l != 0 : v is double d ? d != 0 : v is JObj j ? j.items.Count > 0 : v is List<object> a ? a.Count > 0 : v != null;
        static List<string> MaterialList(JObj e)
        {
            var o = new List<string>();
            if ((e.Get("props") as JObj)?.Get("MaterialList") is List<object> ml) foreach (object m in ml) o.Add((m as JObj)?.Get("asset") as string);
            return o;
        }
        static JObj AssetRef(string asset, string type) => new JObj().Set("asset", asset).Set("type", type);
        internal static void SetEntityId(UxElem e, long id)
        {
            foreach (var a in e.attrs) if (a.name == "entityId") { var b = new byte[4]; UxBin.PutU32(b, 0, (uint)id); a.raw = b; a.str = null; }
        }
        /// {prop: RSData type} of the Untamed entity type in level.json ('types'[type]['prop_types']).
        internal static Dictionary<string, string> UntamedPropTypes(JObj level, string type)
        {
            var ut = new Dictionary<string, string>(StringComparer.Ordinal);
            if (UxJson.Get(level, "types", type) is JObj tj && tj.Get("prop_types") is List<object> pt)
                foreach (object po in pt) if (po is List<object> pair && pair.Count == 2 && pair[0] is string k) ut[k] = pair[1] as string;
            return ut;
        }
        static void Add(UxAssetMap assets, string name, string type, byte[] b, string src, string origin)
        {
            var old = assets.Get(name, type);
            if (old != null) { if (!UxBin.Same(old.data, b)) throw new UxException(name + "." + type + " offered twice with other bytes"); return; }
            assets.Set(name, type, b, src, origin);
        }

        /// build(level, mode, mat_opts, sources, template_assets, lift, group_number, log): (record, {(name, type): (bytes, source,
        /// origin)}, [Entity Elem]) for mode keep | model; drop -> nothing. matOpts: the build's S4 options.
        public static Result Build(UxContext ctx, string mode, UxMaterialsOptions matOpts, UxSources sources, UxTemplateAssets templateAssets, double lift,
            string groupNumber = "zero", UxPaths paths = null, Action<string> log = null)
        {
            paths = paths ?? new UxPaths();
            var lv = ctx.lv; var stock = ctx.stock;
            var d = ctx.work.ReadJsonObj("level.json");
            var ads = new List<JObj>();
            foreach (object eo in UxJson.Arr(d.Get("entities"))) { var e = UxJson.Obj(eo); if (e.Get("type") as string == "MassiveAdEntityRSData") ads.Add(e); }
            UxNp.StableSort(ads, (a, b) => UxJson.Int(a.Get("id")).CompareTo(UxJson.Int(b.Get("id"))));
            var panels = new List<object>();
            var res = new Result { record = new JObj().Set("schema", SCHEMA).Set("option", mode).Set("panels", panels) };
            var rec = res.record;
            if (mode == "drop" || ads.Count == 0)
            {
                rec.Set("reason", mode == "drop" ? "--massive-ads drop" : "no Untamed MassiveAd panel in this level");
                return res;
            }
            var names = new List<string>();
            foreach (var e in ads) foreach (string m in MaterialList(e)) if (!names.Contains(m)) names.Add(m);
            var added = new List<object>();
            var opts = matOpts ?? new UxMaterialsOptions();
            var planner = CtxWith(ctx, opts, names, added);
            rec.Set("closure_added", added);
            var assets = res.assets;
            var matAsset = new Dictionary<string, string>(StringComparer.Ordinal); var diffuse = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string n in names)
            {
                JObj e;
                var se = sources.material_entries != null ? sources.material_entries.Get(n) : null;
                if (se != null && se.Get("action") as string == "convert") { matAsset[n] = se.Get("asset") as string; e = se; }      // S4 already converted it
                else
                {
                    e = planner.Plan(n);
                    if (e.Get("action") as string != "convert") throw new UxException("panel material " + n + ": " + e.Get("action") + " (" + (e.Get("reason") ?? "None") + ")");
                    var (a, how) = UxMaterials.AssetName(stock, n, "material");
                    if (how == "template") throw new UxException("panel material " + n + " is held by the template");
                    var tex = new List<string>(); foreach (object t in UxJson.Arr(e.Get("textures"))) tex.Add(t as string);
                    List<JObj> sp = null;
                    if (e.Get("sampler_patch") is List<object> spl) { sp = new List<JObj>(); foreach (object x in spl) sp.Add(x as JObj); }
                    string template = e.Get("template") as string;
                    byte[] data = UxMaterials.BuildMaterial(stock, template, tex, e.Get("state_patch") as JObj, sp);
                    Add(assets, a, "material", data, paths.Extract(lv, "material/" + n + ".material"), "ux_ads / ux_materials.plan_material + build_material (" + template + ")");
                    matAsset[n] = a;
                    if (Truthy(e.Get("generated_textures")) || Truthy(e.Get("library_textures")))
                        throw new UxException("panel material " + n + " needs generated / library textures (not handled)");
                    var srcTex = UxJson.Arr(e.Get("source_textures"));
                    for (int i = 0; i < Math.Min(srcTex.Count, tex.Count); i++)
                    {
                        string c = srcTex[i] as string, t = tex[i];
                        if (c == null || sources.Has(t, "texture")) continue;              // template / converted closure texture: the builder adds it
                        if (!ctx.disc.HasExtract(c, "texture")) throw new UxException("panel texture " + c + " not in extract/");
                        byte[] sb = ctx.disc.ReadExtract(c, "texture", ctx.cancel);
                        var cv = UxTextures.ConvertTexture(sb, t, templates: stock.templateNames);
                        if (cv.pc == null) throw new UxException("panel texture " + c + " not converted: " + (cv.rec.Get("reason") ?? "None"));
                        var pr = UxTextures.CheckTexture(sb, cv.pc, cv.rec, cv.expect, out _);
                        pr.AddRange(UxTextures.PcLayoutProblems(cv.pc));
                        if (pr.Count > 0) throw new UxException("panel texture " + c + ": " + string.Join("; ", pr.GetRange(0, Math.Min(3, pr.Count))));
                        Add(assets, t, "texture", cv.pc, paths.Extract(lv, "texture/" + c + ".texture"), "ux_ads / ux_textures.convert_texture");
                    }
                }
                var et = UxJson.Arr(e.Get("textures"));
                diffuse[n] = (e.Get("template") as string == "Template_Prelit" && et.Count > 1 ? et[1] : et[0]) as string;
            }
            UxElem proto = mode == "keep" ? stock.Prototype("MassiveAdEntityRSData").entity : stock.LibPrototype("ModelRSData").entity;
            var psig = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var kv in UxEntities.PropSignature(proto)) psig[kv.Key] = kv.Value;
            var ML = new UxMeshLevel(ctx.disc, d);
            foreach (var e in ads)
            {
                var ml = MaterialList(e);
                var eprops = UxJson.Obj(e.Get("props"));
                string mname = UxJson.Get(eprops, "ModelAsset", "asset") as string;
                var mats = new List<string>(); foreach (string m in ml) mats.Add(matAsset[m]);
                var r = UxMeshes.ConvertModel(ML, mname, false, null, null, null, null, mats, "all", ctx.cancel);
                if (r.skipped != null || r.parts.Count != 1) throw new UxException("panel model " + mname + ": " + (r.skipped ?? r.parts.Count + " parts"));
                var p = r.parts[0];
                foreach (string t in new[] { "model", "surface" })
                {
                    if (templateAssets.Clash(p.part, t) != null) throw new UxException(p.part + "." + t + " clashes with the template asset");
                    if (sources.Has(p.part, t)) throw new UxException(p.part + "." + t + " clashes with a converted asset");
                    Add(assets, p.part, t, t == "model" ? p.model_bytes : p.surface_bytes, paths.Extract(lv, t + "/" + mname + "." + t), "ux_ads / ux_meshes.convert_model (render-only)");
                }
                var ut = UntamedPropTypes(d, e.Get("type") as string);
                var props = new JObj();
                foreach (string k in COPY_PROPS)
                    if (psig.ContainsKey(k) && eprops.Has(k) && ut.TryGetValue(k, out string uk) && uk == psig[k]) props.Set(k, eprops.Get(k));
                if (groupNumber == "zero" && psig.ContainsKey("GroupNumber")) props.Set("GroupNumber", 0L);
                foreach (string k in new[] { "RenderIntoStaticShadow", "RenderIntoVolumeShadow" }) if (psig.ContainsKey(k)) props.Set(k, 0L);
                props.Set("ModelAsset", AssetRef(p.model, "model"));
                props.Set("SurfaceAsset", AssetRef(p.surface, "surface"));
                var mlist = new List<object>(); foreach (int k in p.material_index_map) mlist.Add(AssetRef(matAsset[ml[k]], "material"));
                props.Set("MaterialList", mlist);
                if (mode == "keep") props.Set("Ad Texture", AssetRef(diffuse[ml[0]], "texture"));
                long eid = UxJson.Int(e.Get("id")) + UxEntities.ID_OFFSET;
                var m16 = UxJson.Arr(e.Get("matrix")); var mm = new double[m16.Count];
                for (int i = 0; i < mm.Length; i++) mm[i] = UxEntities.Py.Num(m16[i]);
                double[] matrix = UxEntities.Lifted(mm, p.origin_offset, lift);
                var el = proto.DeepCopy();
                SetEntityId(el, eid);
                var P = UxEntities.PropsOf(el);
                var extra = new List<string>(); foreach (var kv in props.items) if (!P.Has(kv.Key)) extra.Add(kv.Key);
                if (extra.Count > 0) throw new UxException((mode == "keep" ? "MassiveAd" : "ModelRSData") + " prototype lacks [" + string.Join(", ", extra) + "]");
                UxEntities.SetValue(P["Name"], e.Get("name")); UxEntities.SetValue(P["Id"], eid); UxEntities.SetValue(P["Matrix"], matrix);
                foreach (var kv in props.items) UxEntities.SetValue(P[kv.Key], kv.Value);
                res.elems.Add(el);
                var rm = new List<object>(); foreach (double x in matrix) rm.Add(UxNp.PyRound(x, 5));
                panels.Add(new JObj().Set("untamed_id", e.Get("id")).Set("id", eid).Set("name", e.Get("name"))
                    .Set("type", mode == "keep" ? "MassiveAdEntityRSData" : "ModelRSData").Set("model", p.model).Set("surface", p.surface)
                    .Set("material", (mlist[0] as JObj).Get("asset")).Set("ad_texture", (props.Get("Ad Texture") as JObj)?.Get("asset"))
                    .Set("draw_distance", p.draw_distance).Set("matrix", rm).Set("tris", (long)p.tris));
            }
            var ids = new List<object>(); var names2 = new List<string>();
            foreach (object x in panels) { ids.Add(((JObj)x).Get("id")); names2.Add(((JObj)x).Get("name") as string); }
            rec.Set("entity_ids", ids);
            var arows = new List<object>();
            foreach (var kv in assets)
                arows.Add(new JObj().Set("name", kv.Key.name).Set("type", kv.Key.type).Set("bytes", (long)kv.Value.data.Length).Set("md5", UxBin.Md5(kv.Value.data)).Set("origin", kv.Value.origin));
            rec.Set("assets", arows);
            log?.Invoke("MassiveAd panels (" + mode + "): " + res.elems.Count + " [" + string.Join(", ", names2) + "], " + assets.Count + " assets");
            return res;
        }
    }
}

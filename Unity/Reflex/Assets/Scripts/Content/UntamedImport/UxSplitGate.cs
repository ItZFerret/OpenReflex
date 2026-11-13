// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_splitgate.py (DESIGN 22): the Untamed split
// start gate for a Reflex sx slot (ux_build_track --split-gate untamed; of the 12 current builds only Foothills uses it).
// UxSplitGate.Build converts, read from the work store's level.json (race_poses; never hard-coded):
//   * the model of the Untamed DropGate entity as a static, collision-bearing model (draw distance 0, groups split in-model), for the
//     base's drop gate entity: Result.dropgate_props (ModelAsset, SurfaceAsset, MaterialList);
//   * the Untamed TO_StartingGate base as a static render-only model (rigid = 'bound') and one ModelRSData for it at the Untamed pose
//     + the level lift (Result.elems), named after its model (never 'TO_StartingGate');
//   * their materials with UxMaterials.Convert(names = ..., the build's S4 options) into <work store>/splitgate/materials[-tag]/
//     (materials.json there), and the textures and shaders those materials name.
// Returns the record, the assets {(name, type): bytes} with their (source, origin), the drop-gate props and the entities. Nothing is
// written outside the work store's splitgate/ folder. Environment fields: record.tool_md5 is the md5 of the reference's ux_splitgate.py
// (UxSplitGate.TOOL_MD5 here), the path texts come from UxPaths. Pure C# (System only).
using System;
using System.Collections.Generic;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_splitgate.py.
    public static class UxSplitGate
    {
        public const string SCHEMA = "ux_splitgate/1", TOOL = "ux_splitgate.py";
        /// md5 of the reference's ux_splitgate.py this port mirrors (2026-11-11): what Python writes as record.tool_md5.
        public const string TOOL_MD5 = "0f52a0596483af6e2f5b490edded0304";

        /// What Build returns: rec (without the '_' keys), rec['_assets'] + rec['_sources'], rec['_dropgate_props'], rec['_entities'].
        public sealed class Result
        {
            public JObj record, dropgate_props;
            public UxAssetMap assets = new UxAssetMap();
            public List<UxElem> elems = new List<UxElem>();
        }

        /// untamed_gate_records(d): (the Untamed DropGate entity record, the Untamed TO_StartingGate entity record) of a level.json.
        public static (JObj dropgate, JObj startgate) UntamedGateRecords(JObj d)
        {
            var rp = d.Get("race_poses") as JObj ?? new JObj();
            var byId = new Dictionary<long, JObj>();
            foreach (object eo in UxJson.Arr(d.Get("entities"))) { var e = UxJson.Obj(eo); if (e.Get("id") is long id) byId[id] = e; }
            JObj Find(string role) => (rp.Get(role) as JObj)?.Get("id") is long id && byId.TryGetValue(id, out var e) ? e : null;
            JObj dg = Find("DropGate"), sg = Find("TO_StartingGate");
            if (dg == null || !Truthy((dg.Get("props") as JObj)?.Get("ModelAsset"))) throw new UxException("no Untamed DropGate entity with a ModelAsset in level.json race_poses");
            if (sg == null || !Truthy((sg.Get("props") as JObj)?.Get("ModelAsset"))) throw new UxException("no Untamed TO_StartingGate entity with a ModelAsset in level.json race_poses");
            return (dg, sg);
        }
        static bool Truthy(object v) => v is string s ? s.Length > 0 : v is bool b ? b : v is long l ? l != 0 : v is double d ? d != 0 : v is JObj j ? j.items.Count > 0 : v is List<object> a ? a.Count > 0 : v != null;
        static List<string> MList(JObj e)
        {
            object ml = (e.Get("props") as JObj)?.Get("MaterialList");
            var o = new List<string>();
            if (!Truthy(ml)) return o;
            if (ml is List<object> list) foreach (object x in list) o.Add((x as JObj)?.Get("asset") as string);
            else o.Add((ml as JObj)?.Get("asset") as string);
            return o;
        }
        static List<object> Objs(IEnumerable<string> s) { var l = new List<object>(); foreach (string x in s) l.Add(x); return l; }
        static JObj AssetRef(string asset, string type) => new JObj().Set("asset", asset).Set("type", type);

        /// out_dir(lv, mat_opts) relative to the work store: 'splitgate/materials[-<tag>]'.
        public static string OutDir(UxMaterialsOptions matOpts)
        {
            string tag = matOpts != null ? matOpts.Tag() : "";
            return "splitgate/materials" + (tag.Length > 0 ? "-" + tag : "");
        }

        /// build(level, mat_opts, sources, template_assets, lift, group_number, log).
        public static Result Build(UxContext ctx, UxMaterialsOptions matOpts, UxSources sources, UxTemplateAssets templateAssets, double lift, string groupNumber = "zero",
            UxPaths paths = null, Action<string> log = null)
        {
            paths = paths ?? new UxPaths();
            matOpts = matOpts ?? new UxMaterialsOptions();
            var lv = ctx.lv; var work = ctx.work; var stock = ctx.stock;
            byte[] lj = work.ReadBytes("level.json");
            var d = UxJson.Obj(UxJson.ParseUtf8(lj));
            var (dg, sg) = UntamedGateRecords(d);
            JObj dgProps = UxJson.Obj(dg.Get("props")), sgProps = UxJson.Obj(sg.Get("props"));
            var res = new Result();
            var rec = new JObj().Set("schema", SCHEMA).Set("tool", TOOL).Set("tool_md5", TOOL_MD5).Set("level", lv.disc).Set("option", "untamed")
                .Set("level_json_md5", UxBin.Md5(lj))
                .Set("untamed_dropgate", new JObj().Set("id", dg.Get("id")).Set("name", dg.Get("name")).Set("type", dg.Get("type"))
                    .Set("model", UxJson.Get(dgProps, "ModelAsset", "asset")).Set("materials", Objs(MList(dg))).Set("collision", (dgProps.Get("FemaleCollisionAsset") as JObj)?.Get("asset")))
                .Set("untamed_startinggate", new JObj().Set("id", sg.Get("id")).Set("name", sg.Get("name")).Set("type", sg.Get("type"))
                    .Set("model", UxJson.Get(sgProps, "ModelAsset", "asset")).Set("materials", Objs(MList(sg))));
            res.record = rec;
            var assets = res.assets;
            void Add(string name, string type, byte[] b, string src, string origin)
            {
                var old = assets.Get(name, type);
                if (old != null) { if (!UxBin.Same(old.data, b)) throw new UxException(name + "." + type + " offered twice with other bytes"); return; }
                assets.Set(name, type, b, src, origin);
            }

            // ---------------------------------------------------------------- materials (the build's S4 options)
            var names = new List<string>();
            foreach (string n in MList(dg)) if (!string.IsNullOrEmpty(n) && !names.Contains(n)) names.Add(n);
            foreach (string n in MList(sg)) if (!string.IsNullOrEmpty(n) && !names.Contains(n)) names.Add(n);
            string od = OutDir(matOpts);
            var mo = new UxMaterialsOptions
            {
                alpha = matOpts.alpha, prelit_off = matOpts.prelit_off, nrm_off = matOpts.nrm_off, crowd_off = matOpts.crowd_off, blend = matOpts.blend, bale_base = matOpts.bale_base,
                mipfilter = matOpts.mipfilter, composite = matOpts.composite, literal_albedo = matOpts.literal_albedo, alpha_source = matOpts.alpha_source,
                diffuse_pick = matOpts.diffuse_pick, names = names, out_dir = od, write = true, jobs = matOpts.jobs,
                tool_md5 = matOpts.tool_md5, lib_index_md5 = matOpts.lib_index_md5, sm3dis_md5 = matOpts.sm3dis_md5, readers = matOpts.readers,
            };
            var quiet = new UxContext { lv = ctx.lv, disc = ctx.disc, library = ctx.library, stock = ctx.stock, work = ctx.work, cancel = ctx.cancel };
            var man = UxMaterials.Convert(quiet, mo);
            if (man.Get("problems") is List<object> mp && mp.Count > 0)
                throw new UxException("ux_materials on the gate materials: " + UxJson.Dumps(mp.GetRange(0, Math.Min(5, mp.Count)), -1, false, false));
            var ents = new Dictionary<string, JObj>(StringComparer.Ordinal);
            foreach (var kv in UxMaterials.Entries(man)) ents[kv.Key] = kv.Value;
            var matAsset = new Dictionary<string, string>(StringComparer.Ordinal);
            var gen = man.Get("generated_textures") as JObj ?? new JObj(); var lib = man.Get("library_textures") as JObj ?? new JObj();
            foreach (string n in names)
            {
                ents.TryGetValue(n, out var e);
                string action = e?.Get("action") as string;
                if (e == null || action != "convert" && action != "template")
                    throw new UxException("gate material " + n + ": " + (action ?? "None") + " (" + (e?.Get("reason") ?? "None") + ")");
                matAsset[n] = e.Get("asset") as string;
                if (action == "template") continue;
                string f = od + "/" + (e.Get("file") as string);
                byte[] b = work.ReadBytes(f);
                if (UxBin.Md5(b) != e.Get("md5") as string) throw new UxException(paths.Work(lv, f) + " md5 differs from its manifest");
                Add(matAsset[n], "material", b, paths.Work(lv, f), "ux_splitgate / ux_materials.convert (" + n + ")");
                // textures
                var srcTex = UxJson.Arr(e.Get("source_textures")); var tex = UxJson.Arr(e.Get("textures"));
                for (int i = 0; i < Math.Min(srcTex.Count, tex.Count); i++)
                {
                    string c = srcTex[i] as string, a = tex[i] as string;
                    if (gen.Has(a) || lib.Has(a))
                    {
                        var meta = (Truthy(gen.Get(a)) ? gen.Get(a) : lib.Get(a)) as JObj;
                        string p = od + "/" + (meta.Get("file") as string);
                        byte[] tb = work.ReadBytes(p);
                        if (Truthy(meta.Get("md5")) && UxBin.Md5(tb) != meta.Get("md5") as string) throw new UxException(paths.Work(lv, p) + " md5 differs from its manifest");
                        Add(a, "texture", tb, paths.Work(lv, p), "ux_splitgate / ux_materials " + (gen.Has(a) ? "generated" : "library") + " texture");
                    }
                    else if (c == null)
                    {
                        if (!templateAssets.Has(a, "texture")) throw new UxException("material " + n + " names template texture " + a + ", not in the template");
                    }
                    else if (sources.Has(a, "texture"))
                    {
                        var sa = sources.assets[(a, "texture")];
                        Add(a, "texture", sources.Read((a, "texture")), sources.PyPath(sa.file), sa.origin);
                    }
                    else
                    {
                        if (!ctx.disc.HasExtract(c, "texture"))
                            throw new UxException("texture " + a + " (" + c + ") neither converted by the texture stage nor in " + paths.Extract(lv, "texture"));
                        byte[] sb = ctx.disc.ReadExtract(c, "texture", ctx.cancel);
                        var cv = UxTextures.ConvertTexture(sb, a, templates: stock.templateNames);
                        if (cv.pc == null) throw new UxException("texture " + c + " not converted: " + (cv.rec.Get("reason") ?? "None"));
                        var pr = UxTextures.CheckTexture(sb, cv.pc, cv.rec, cv.expect, out _);
                        pr.AddRange(UxTextures.PcLayoutProblems(cv.pc));
                        if (pr.Count > 0) throw new UxException("texture " + c + ": " + string.Join("; ", pr.GetRange(0, Math.Min(3, pr.Count))));
                        Add(a, "texture", cv.pc, paths.Extract(lv, "texture/" + c + ".texture"), "ux_splitgate / ux_textures.convert_texture");
                    }
                }
                string T = e.Get("template") as string;
                if (templateAssets.Has(T, "shader")) { }
                else if (sources.Has(T, "shader"))
                {
                    var sa = sources.assets[(T, "shader")];
                    Add(T, "shader", sources.Read((T, "shader")), sources.PyPath(sa.file), sa.origin);
                }
                else
                {
                    string p = od + "/shaders/" + T + ".shader";
                    Add(T, "shader", work.ReadBytes(p), paths.Work(lv, p), "ux_splitgate / ux_materials library shader");
                }
            }
            var mrec = new JObj();
            foreach (string n in names)
                mrec.Set(n, new JObj().Set("asset", matAsset[n]).Set("action", ents[n].Get("action")).Set("template", ents[n].Get("template")).Set("textures", ents[n].Get("textures"))
                    .Set("reason", ents[n].Get("reason")));
            rec.Set("materials", mrec);
            rec.Set("materials_manifest", new JObj().Set("path", paths.Work(lv, od + "/materials.json")).Set("md5", UxBin.Md5(work.ReadBytes(od + "/materials.json"))));

            // ---------------------------------------------------------------- models
            var ML = new UxMeshLevel(ctx.disc, d);
            (UxMeshPart part, JObj info) Model(JObj e, bool collisionBearing, string rigid)
            {
                string name = UxJson.Get(e.Get("props"), "ModelAsset", "asset") as string;
                var ml = new List<string>(); foreach (string n in MList(e)) ml.Add(matAsset[n]);
                var r = UxMeshes.ConvertModel(ML, name, collisionBearing, null, null, null, null, ml, rigid, ctx.cancel);
                if (r.skipped != null) throw new UxException("model " + name + ": " + r.skipped);
                if (r.parts.Count != 1) throw new UxException("model " + name + " converts to " + r.parts.Count + " parts (one entity needs one part)");
                var p = r.parts[0];
                foreach (int v in p.group_verts) if (v > UxCommon.Limits.MaxGroupVerts) throw new UxException("model " + name + ": a group above the limits (" + v + " vertices)");
                foreach (int v in p.group_indices) if (v > UxCommon.Limits.MaxGroupStripIndices) throw new UxException("model " + name + ": a group above the limits (" + v + " strip indices)");
                foreach (string t in new[] { "model", "surface" })
                {
                    var cl = templateAssets.Clash(p.part, t);
                    if (cl != null) throw new UxException(p.part + "." + t + " clashes with the template asset " + cl.name);
                    Add(p.part, t, t == "model" ? p.model_bytes : p.surface_bytes, paths.Extract(lv, t + "/" + name + "." + t),
                        "ux_splitgate / ux_meshes.convert_model(" + (collisionBearing ? "collision-bearing" : "render-only, rigid=" + rigid) + ")");
                }
                var info = new JObj().Set("model", p.model).Set("surface", p.surface).Set("source", name).Set("groups", p.groups).Set("material_index_map", p.material_index_map)
                    .Set("materials", p.materials).Set("group_verts", p.group_verts).Set("group_indices", p.group_indices).Set("tris", (long)p.tris)
                    .Set("draw_distance", p.draw_distance).Set("origin_offset", p.origin_offset).Set("split_groups", r.info.Get("split_groups"))
                    .Set("rigid_static", r.info.Get("rigid_static"))
                    .Set("md5", new JObj().Set("model", UxBin.Md5(p.model_bytes)).Set("surface", UxBin.Md5(p.surface_bytes)));
                return (p, info);
            }
            var (pd, dinfo) = Model(dg, true, "all");
            rec.Set("dropgate_model", dinfo);
            var dgml = MList(dg); var dgList = new List<object>();
            foreach (int k in pd.material_index_map) dgList.Add(AssetRef(matAsset[dgml[k]], "material"));
            var dgp = new JObj().Set("ModelAsset", AssetRef(pd.model, "model")).Set("SurfaceAsset", AssetRef(pd.surface, "surface")).Set("MaterialList", dgList);
            res.dropgate_props = dgp;
            var dgpCopy = new JObj(); foreach (var kv in dgp.items) dgpCopy.Set(kv.Key, kv.Value);
            rec.Set("dropgate_props", dgpCopy);
            var (pb, binfo) = Model(sg, false, "bound");
            rec.Set("base_model", binfo);

            // ---------------------------------------------------------------- the base entity (render-only ModelRSData)
            var protos = new UxEntities.Prototypes(stock);
            var psig = protos.Signature("ModelRSData");
            var ut = UxAds.UntamedPropTypes(d, sg.Get("type") as string);
            var props = new JObj();
            foreach (string k in UxAds.COPY_PROPS)
                if (psig.ContainsKey(k) && sgProps.Has(k) && ut.TryGetValue(k, out string uk) && uk == psig[k]) props.Set(k, sgProps.Get(k));
            if (groupNumber == "zero" && psig.ContainsKey("GroupNumber")) props.Set("GroupNumber", 0L);
            props.Set("RenderIntoStaticShadow", 0L);
            props.Set("RenderIntoVolumeShadow", 0L);
            props.Set("LockScale", 0L);
            props.Set("ModelAsset", AssetRef(pb.model, "model"));
            props.Set("SurfaceAsset", AssetRef(pb.surface, "surface"));
            var sgml = MList(sg); var sgList = new List<object>();
            foreach (int k in pb.material_index_map) sgList.Add(AssetRef(matAsset[sgml[k]], "material"));
            props.Set("MaterialList", sgList);
            string ename = pb.model;
            if (ename == "TO_StartingGate") throw new UxException("the base entity may not be named TO_StartingGate");
            var m16 = UxJson.Arr(sg.Get("matrix")); var mm = new double[m16.Count];
            for (int i = 0; i < mm.Length; i++) mm[i] = UxEntities.Py.Num(m16[i]);
            var spec = new UxEntities.Spec { id = UxJson.Int(sg.Get("id")) + UxEntities.ID_OFFSET, name = ename, type = "ModelRSData", subscene = "Base",
                matrix = UxEntities.Lifted(mm, pb.origin_offset, lift), props = props };
            var elem = UxEntities.SpecToElem(spec, protos);
            var propsCopy = new JObj(); foreach (var kv in props.items) propsCopy.Set(kv.Key, kv.Value);
            rec.Set("base_entity", new JObj().Set("id", spec.id).Set("name", ename).Set("type", "ModelRSData").Set("matrix", spec.matrix).Set("props", propsCopy)
                .Set("rule", "Untamed TO_StartingGate pose + " + UxEntities.Py.G(lift) + " ft lift; render-only (Untamed: ModelRSData without collision); GateStart nodes ignored"));
            rec.Set("entity_ids", new List<object> { spec.id });
            var arows = new List<object>();
            foreach (var kv in assets)
                arows.Add(new JObj().Set("name", kv.Key.name).Set("type", kv.Key.type).Set("bytes", (long)kv.Value.data.Length).Set("md5", UxBin.Md5(kv.Value.data))
                    .Set("source", kv.Value.source).Set("origin", kv.Value.origin));
            rec.Set("assets", arows);
            log?.Invoke("split gate: " + UxJson.Get(dgProps, "ModelAsset", "asset") + " -> the drop gate (" + pd.groups.Length + " groups), " + UxJson.Get(sgProps, "ModelAsset", "asset") +
                " -> " + ename + " (id " + spec.id + "), " + assets.Count + " assets");
            res.elems.Add(elem);
            return res;
        }
    }
}

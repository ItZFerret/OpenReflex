// MX vs ATV Untamed converter, foundation: the stock Reflex content the converter builds on, read from the player's own Reflex library
// (IUxLibrary: an original install or the client's game files). The Python reference keeps copies under templates/ (stock-lib, reflex-lib,
// stock-SX_FortDodge), built once from the install by ux_stocklib.py --fetch, reflex-lib/build_lib.py and ux_base.ensure_stock_copy; here
// the same bytes are produced on demand and checked against the same md5 pins. Only names, source levels, sizes and md5s are in this file.
//   ux_stocklib   UxStock.Has / Get / Record / Prototype          (tuff blocks, the water kit, the two base-gap textures, 2 prototypes)
//   ux_common     UxStock.LibShaderBytes / LibMaterialBytes / LibPrototype / LibTemplate   (reflex-lib: 8 shaders, 8 exemplar materials,
//                 9 entity prototypes; index.json's per-template facts the materials stage reads)
//   ux_base       UxStock.StockPins / BaseFile / BaseSet            (the clone base SX_FortDodge: its four files)
//   templates     UxTemplateNames                                   (asset names of a template database: the stages' clash checks;
//                 UxTemplateNames.Pinned: the constant tables a UxStock carries, so no template database is needed)
using System;
using System.Collections.Generic;
using System.IO;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// templates/stock-lib/index.json 'assets' row: '<name>.<type>' -> source level, md5, bytes, in_fortdodge.
    public sealed class UxStockAsset
    {
        public readonly string key, name, type, source, md5; public readonly int bytes; public readonly bool inFortDodge;
        public UxStockAsset(string key, string source, string md5, int bytes, bool inFortDodge)
        {
            this.key = key; int dot = key.LastIndexOf('.'); name = key.Substring(0, dot); type = key.Substring(dot + 1);
            this.source = source; this.md5 = md5; this.bytes = bytes; this.inFortDodge = inFortDodge;
        }
        /// The index.json record (key order as ux_stocklib writes it; write_json sorts keys anyway).
        public JObj Record() => new JObj().Set("source", source).Set("md5", md5).Set("bytes", (long)bytes).Set("in_fortdodge", inFortDodge);
    }

    /// A one-entity SceneChunk prototype (stock-lib and reflex-lib index.json 'prototypes' rows): where the stock entity is.
    public sealed class UxStockPrototype
    {
        public readonly string rsdata, file, level, subScene, md5; public readonly int chunk, indexInChunk, bytes; public readonly long entityId;
        public UxStockPrototype(string rsdata, string file, string level, string subScene, int chunk, int indexInChunk, long entityId, string md5, int bytes)
        {
            this.rsdata = rsdata; this.file = file; this.level = level; this.subScene = subScene; this.chunk = chunk; this.indexInChunk = indexInChunk;
            this.entityId = entityId; this.md5 = md5; this.bytes = bytes;
        }
        public string SourceScene => level + ".dx9.scene";
    }

    /// templates/reflex-lib/index.json 'templates' row (the facts the materials stage reads; the analysis lists are not kept).
    public sealed class UxLibTemplate
    {
        public readonly string name, sourceSet, sourcePackage, exemplar, exemplarPackage, shaderMd5, exemplarMd5, shadersForm, vsEntry, vsSkinEntry, psEntry;
        public readonly int shaderBytes, exemplarBytes, numTextures;
        /// textures: the exemplar's texture names; userSamplers: user_samplers_register_order; samplers: samplers_register_order as
        /// (register 's0', sampler name) pairs.
        public readonly string[] textures, userSamplers; public readonly KeyValuePair<string, string>[] samplers;
        public UxLibTemplate(string name, string sourceSet, string sourcePackage, string exemplar, string exemplarPackage, string shaderMd5, int shaderBytes,
            string exemplarMd5, int exemplarBytes, string shadersForm, string vsEntry, string vsSkinEntry, string psEntry, int numTextures,
            string[] textures, string[] userSamplers, string[] samplerPairs)
        {
            this.name = name; this.sourceSet = sourceSet; this.sourcePackage = sourcePackage; this.exemplar = exemplar; this.exemplarPackage = exemplarPackage;
            this.shaderMd5 = shaderMd5; this.shaderBytes = shaderBytes; this.exemplarMd5 = exemplarMd5; this.exemplarBytes = exemplarBytes;
            this.shadersForm = shadersForm; this.vsEntry = vsEntry; this.vsSkinEntry = vsSkinEntry; this.psEntry = psEntry; this.numTextures = numTextures;
            this.textures = textures; this.userSamplers = userSamplers;
            samplers = new KeyValuePair<string, string>[samplerPairs.Length / 2];
            for (int i = 0; i < samplers.Length; i++) samplers[i] = new KeyValuePair<string, string>(samplerPairs[2 * i], samplerPairs[2 * i + 1]);
        }
        /// The index.json fields this row carries, in a JObj (lists as List<object>; samplers_register_order as [register, name] pairs).
        public JObj Index()
        {
            var sro = new List<object>(); foreach (var kv in samplers) sro.Add(new List<object> { kv.Key, kv.Value });
            return new JObj().Set("source_set", sourceSet).Set("source_package", sourcePackage).Set("exemplar", exemplar).Set("exemplar_package", exemplarPackage)
                .Set("shader_md5", shaderMd5).Set("shader_bytes", (long)shaderBytes).Set("exemplar_md5", exemplarMd5).Set("exemplar_bytes", (long)exemplarBytes)
                .Set("shaders_form", shadersForm).Set("vs_entry", vsEntry).Set("vs_skin_entry", vsSkinEntry).Set("ps_entry", psEntry)
                .Set("num_textures", (long)numTextures).Set("textures", new List<object>(textures)).Set("user_samplers_register_order", new List<object>(userSamplers))
                .Set("samplers_register_order", sro).Set("material_file", "materials/" + name + ".material");
        }
    }

    /// {type: {lower name: name}} of the assets a template database lists (ux_textures.template_texture_names, ux_collision.
    /// template_collis_names, ux_materials.template_asset_names): the stages' name-clash checks.
    public sealed class UxTemplateNames
    {
        /// type -> lower name -> name, in database order (a repeated lower name keeps the LAST spelling, as the Python dict assignment).
        public readonly Dictionary<string, Dictionary<string, string>> byType = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        public readonly string slot;
        public UxTemplateNames(string slot, byte[] database)
        {
            this.slot = slot;
            var db = new UxBxml(database);
            int pk = db.Find(0, "Packages");
            UxBin.Require(pk >= 0, "template database without Packages");
            foreach (int p in db.FindAll(pk, "Package"))
                foreach (int a in db.FindAll(p, "Asset"))
                {
                    string type = db.AttrStr(a, "type"), name = db.AttrStr(a, "name");
                    if (!byType.TryGetValue(type, out var d)) byType[type] = d = new Dictionary<string, string>(StringComparer.Ordinal);
                    d[UxCommon.PyLower(name)] = name;
                }
        }
        /// A constant table: (type, name) pairs.
        public UxTemplateNames(string slot, IEnumerable<(string type, string name)> names)
        {
            this.slot = slot;
            foreach (var (type, name) in names)
            {
                if (!byType.TryGetValue(type, out var d)) byType[type] = d = new Dictionary<string, string>(StringComparer.Ordinal);
                d[UxCommon.PyLower(name)] = name;
            }
        }
        // The stages (ux_textures, ux_materials, ux_collision) ask the DE sketch slot ('sx': templates/sx-sketch-slot1) and the HuCkiT
        // free-ride slot ('fr': templates/huckit-freeride-slot1) whether a name is taken: a taken name gets the 'UX_' prefix, and an
        // Editor_Default* name both hold is used as it is. The current stock-base build still names its stage outputs by those two
        // tables (the stock base's own names are checked later, by the builder, against the SX_FortDodge package), so they decide
        // asset names. A player's Reflex library has neither slot. These are the only names of the two databases (962 and 341 assets)
        // that any asset of the 12 supported levels, or any fixed name the stages ask for, meets: 17 and 8 names, all stock Reflex
        // names. With them every lookup of the 12 levels gives the full tables' answer.
        static readonly (string type, string name)[] PinnedSx =
        {
            ("collis", "Editor_Default"),
            ("material", "Camera"), ("material", "CameraPersonClothes"), ("material", "Editor_Default"),
            ("shader", "Editor_Default"), ("shader", "Water"),
            ("surface", "BillboardQuad"),
            ("texture", "Camera"), ("texture", "CameraPersonClothes"), ("texture", "Editor_Default"), ("texture", "Editor_Default_Black"),
            ("texture", "Editor_Default_BlackAlpha"), ("texture", "Editor_Default_Gray"), ("texture", "Editor_Default_White"), ("texture", "LightFlare"),
            ("texture", "WaterFoam"), ("texture", "WaterPressure"),
        };
        static readonly (string type, string name)[] PinnedFr =
        {
            ("collis", "Editor_Default"), ("material", "Editor_Default"), ("shader", "Editor_Default"),
            ("texture", "Editor_Default"), ("texture", "Editor_Default_Black"), ("texture", "Editor_Default_BlackAlpha"), ("texture", "Editor_Default_Gray"),
            ("texture", "Editor_Default_White"),
        };
        /// The constant name table of a template kind ('sx' or 'fr'): what the pipeline uses when it has only the disc and the stock
        /// Reflex library (UxStock fills templateNames with these).
        public static UxTemplateNames Pinned(string kind)
        {
            switch (kind)
            {
                case "sx": return new UxTemplateNames("Beta_Sx_Track_Slot_1", PinnedSx);
                case "fr": return new UxTemplateNames("Beta_Track_Slot_1", PinnedFr);
            }
            throw new UxException("no pinned template names for kind " + kind);
        }
        static readonly Dictionary<string, string> s_none = new Dictionary<string, string>();
        /// names[type] ({} when the template has no asset of the type).
        public Dictionary<string, string> Of(string type) => byType.TryGetValue(type, out var d) ? d : s_none;
        /// names[type].get(name.lower()).
        public string Find(string type, string name) => Of(type).TryGetValue(UxCommon.PyLower(name), out var n) ? n : null;
    }

    /// The stock content of the converter, from the player's library.
    public sealed class UxStock
    {
        public readonly IUxLibrary library;
        readonly object _gate = new object();
        readonly Dictionary<string, byte[]> _cache = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        /// U.TEMPLATE_DIR's template names by kind ('sx': the DE sketch slot, 'fr': the HuCkiT free-ride slot). Not part of a Reflex library:
        /// filled with the constant tables UxTemplateNames.Pinned (enough for the 12 supported levels); the stage tests replace them
        /// with the reference's full template databases (see core/plan/UNTAMED_CONVERT.md, template names).
        public readonly Dictionary<string, UxTemplateNames> templateNames = new Dictionary<string, UxTemplateNames>(StringComparer.Ordinal);

        public UxStock(IUxLibrary library)
        {
            this.library = library;
            templateNames["sx"] = UxTemplateNames.Pinned("sx"); templateNames["fr"] = UxTemplateNames.Pinned("fr");
        }

        static UxStockAsset A(string key, string source, string md5, int bytes, bool inFd) => new UxStockAsset(key, source, md5, bytes, inFd);

        // ------------------------------------------------------------------------------------------ ux_stocklib
        /// templates/stock-lib/index.json 'assets', in the file's (sorted) order.
        public static readonly UxStockAsset[] Assets =
        {
            A("Editor_Default_BlackAlpha.texture", "FR_Gibson", "614640fef5177a31581c9222d2b5b9b3", 1496, false),
            A("Editor_Default_Gray.texture", "SX_Manchester", "e540c2e8d3e4aaa5b20ffade49962a1b", 68, false),
            A("FR_Gibson_Particles.partattr", "FR_Gibson", "3dd963e4db651b1e784bd3598b8b9ef3", 59590, false),
            A("FR_Gibson_Particles.particle", "FR_Gibson", "65b6ea7021617769a897f62e586bb014", 28619, false),
            A("FR_Gibson_Particles.texture", "FR_Gibson", "0919ace7de5a432d57343687187291d6", 1398272, false),
            A("GL_HB_Corner_FNL.texture", "SX_FortDodge", "ef153d1a14b53ba47e1d07e3df187f60", 699188, true),
            A("GL_HB_Double_FNL.texture", "SX_FortDodge", "894805627c2eba251708884eab790eae", 699188, true),
            A("GL_HB_Single_FNL.texture", "SX_FortDodge", "9e0acc6b201a0feb71676afcebdaa7da", 699188, true),
            A("GL_HB_Wedge_FNL.texture", "SX_FortDodge", "eb5b33d03e2a211af9cb855ad133cd02", 699188, true),
            A("GL_HayBale_Corner.model", "SX_FortDodge", "47052ca8bd1ed6c4206c5bbf44e4a8b1", 356, true),
            A("GL_HayBale_Corner.surface", "SX_FortDodge", "314128aba2e286db7f46e39224103156", 4336, true),
            A("GL_HayBale_Corner_COL.collis", "SX_FortDodge", "5584d86dc77640eaa460889b4585933e", 820, true),
            A("GL_HayBale_Double.model", "SX_FortDodge", "43d56641be4dbd7f4c735fc42e919738", 358, true),
            A("GL_HayBale_Double.surface", "SX_FortDodge", "525851b8fcfbca81f2024d4b74562739", 3132, true),
            A("GL_HayBale_Double_COL.collis", "SX_FortDodge", "60cccd5c25395f574f06f949ff450039", 116, true),
            A("GL_HayBale_Single.model", "SX_FortDodge", "ae3d32b6dd2f2db173e75842dcdc32ef", 356, true),
            A("GL_HayBale_Single.surface", "SX_FortDodge", "5a06c4f1e5c17e8593d203dae2c71242", 2362, true),
            A("GL_HayBale_Single_COL.collis", "SX_FortDodge", "e8606b0d4e0e60b063a7c0a5b09e16ea", 116, true),
            A("GL_HayBale_Wedge.model", "SX_FortDodge", "7d269f21e22469cad2ebbf39a28e4ef2", 354, true),
            A("GL_HayBale_Wedge.surface", "SX_FortDodge", "2007f6a52f39ce7892d218ade751b2cd", 2834, true),
            A("GL_HayBale_Wedge_COL.collis", "SX_FortDodge", "31783275eba30013cb40304e50bf81ba", 292, true),
            A("GL_Haybale_T1.model", "SX_FortDodge", "8c51c2af209908a495d3a88fe179d491", 352, true),
            A("GL_Haybale_T1.surface", "SX_FortDodge", "90d8019ef54f0dbf4928b46dca570355", 3216, true),
            A("GL_Haybale_T1_COL.collis", "SX_FortDodge", "e188ccac7cc58f01d3c46b77873228ae", 292, true),
            A("HB_Corner_Cernics.material", "SX_FortDodge", "53c24afe4400f3325eb3e336c7ddcea7", 1056, true),
            A("HB_Corner_DG.material", "SX_FortDodge", "fb207411e22c94df3c590c710b333e34", 1051, true),
            A("HB_Corner_FMF.material", "SX_FortDodgeB", "cd114dab992a29f4e1dec1e288032fa2", 1052, false),
            A("HB_Corner_Hiper.material", "SX_FortDodge", "bc678ad063afcba5c5fde2931510f716", 1054, true),
            A("HB_Corner_PitPassRadio.material", "SX_KingstonB", "477b7eda5d3c042b863d07d77e985101", 1061, false),
            A("HB_Corner_ProCircuit.material", "SX_ManchesterB", "46aeb312fea08f8677f93c72541ddbe3", 1059, false),
            A("HB_Corner_ScaryFast.material", "SX_FortDodgeB", "dc664985076dd99d4c146604ceb06e10", 1058, false),
            A("HB_Corner_Shift.material", "SX_FortDodge", "950e188f86ce95ad4b0b9a800b3e952a", 1054, true),
            A("HB_Corner_TroyLeeDesigns.material", "SX_Kingston", "c9ab128adbb72c4cb166db582febdfc8", 1063, false),
            A("HB_Double_AlpineStars.material", "SX_FortDodgeB", "cdc04d624a0dbcb73edb1efa9b72d19b", 1060, false),
            A("HB_Double_Answer.material", "SX_FortDodge", "e84ae860d05202bec8dfcf8dccd5e674", 1055, true),
            A("HB_Double_Bell.material", "SX_Springfield", "1b76ffab8b9c6347a84a13be4ba7553e", 1053, false),
            A("HB_Double_BigGun.material", "FMX_Springfield", "98dd647d7cabf3ce8ce2a17b846b0485", 1055, false),
            A("HB_Double_BlingStar.material", "FMX_BuenaVista", "f38b5b5f0dc44f211762cfffa15ce123", 1058, false),
            A("HB_Double_Cernics.material", "SX_FortDodge", "5abfee7eb932fde56f0cd84b753416a7", 1056, true),
            A("HB_Double_DG.material", "SX_FortDodge", "7aa1e197920807683d8bd36865132c57", 1051, true),
            A("HB_Double_EVS.material", "SX_Manchester", "1d83d944cd8ba626ad31d162830ff19b", 1052, false),
            A("HB_Double_FLY.material", "SX_KingstonB", "34688bed0e4b9fe6068a878651082fd5", 1052, false),
            A("HB_Double_FMF.material", "SX_FortDodgeB", "a08846c44a94b2df94911f051c1a66b6", 1052, false),
            A("HB_Double_Hiper.material", "SX_FortDodge", "16f397fe152430520d0053bce37022a9", 1054, true),
            A("HB_Double_KTM.material", "SX_Kingston", "08c0b724c75baac8d0439bcd622f5bdb", 1052, false),
            A("HB_Double_Kenda.material", "SX_Manchester", "1b4a6b839bbf3d808400ef69db59e9f0", 1054, false),
            A("HB_Double_MSR.material", "SX_KingstonB", "9f6a01245ca622f7e0c341e3c4a935fb", 1052, false),
            A("HB_Double_Oakley.material", "SX_Springfield", "04e01063c8c08363bb50af0c5961c692", 1055, false),
            A("HB_Double_PartsUnlimited.material", "SX_Kingston", "f05cc91f62023530749d7ceed1b8087f", 1063, false),
            A("HB_Double_PitPassRadio.material", "SX_FortDodgeB", "c015c79c46ff4d5a875b74faa83f3100", 1061, false),
            A("HB_Double_ProCircuit.material", "SX_ManchesterB", "9731dba4b294350037684c8a5d1ab9d6", 1059, false),
            A("HB_Double_RaceTech.material", "SX_Manchester", "469ec4f5a92c1a2023bb20d27269d43b", 1057, false),
            A("HB_Double_Renthal.material", "FMX_BuenaVistaB", "d9a963b1a3d0dad17dbc06326e2b82d0", 1056, false),
            A("HB_Double_ScaryFast.material", "SX_FortDodgeB", "41324ddac98847ce005e576708472a7e", 1058, false),
            A("HB_Double_Shift.material", "SX_FortDodge", "23a9e68ff452209a76d9bf66320c04ce", 1054, true),
            A("HB_Double_Sidi.material", "SX_FortDodge", "9fc24da6b6fbcba3556460b0ba077e8f", 1053, true),
            A("HB_Double_Spy.material", "SX_SpringfieldB", "4ce0dec6c1f4ca05379f006132090ea9", 1052, false),
            A("HB_Double_TroyLeeDesigns.material", "SX_Kingston", "d578a8ec5e69e4b8f412261fc4266d06", 1063, false),
            A("HB_Double_Yoshimura.material", "SX_Manchester", "61a6e6ff2768d86198ba4319cc0028ba", 1058, false),
            A("HB_Single_AlpineStars.material", "SX_FortDodgeB", "22977273f5cb16d7a52b9ee478c848a7", 1050, false),
            A("HB_Single_Answer.material", "SX_FortDodge", "decb2bdf89b2d56d968965988ef865fa", 1045, true),
            A("HB_Single_BigGun.material", "NAT_RenneB", "c0b84063215bcdf1dc848c2944f55a18", 1045, false),
            A("HB_Single_BlingStar.material", "FMX_BuenaVistaB", "702fa87396efc9f9b4c394a8178906af", 1048, false),
            A("HB_Single_Cernics.material", "SX_FortDodge", "aa8fc72e44bda35df9e361996c284cab", 1046, true),
            A("HB_Single_DG.material", "SX_FortDodge", "1631dff710adc978030279b48b7b767c", 1041, true),
            A("HB_Single_DuncanRacing.material", "NAT_Bedford", "4617f9e36d59dac53193483c0f8f62f4", 1051, false),
            A("HB_Single_EVS.material", "SX_Springfield", "e26daab8742fe7fa06e3fc9da073f15e", 1042, false),
            A("HB_Single_FLY.material", "SX_KingstonB", "c15908b5fc897f469cee4cae3692eac7", 1042, false),
            A("HB_Single_FMF.material", "SX_FortDodgeB", "4d06dccdf662d459d6c00f85e7953da3", 1042, false),
            A("HB_Single_KTM.material", "SX_Kingston", "97051d23f4d6a117b3f47ee881614489", 1042, false),
            A("HB_Single_Kenda.material", "SX_Manchester", "66f79017a9d2c1dffe738946df3e44e9", 1044, false),
            A("HB_Single_Laegers.material", "FMX_BuenaVistaB", "44918c02fbdbb5d3636aa4918e42c968", 1046, false),
            A("HB_Single_MSR.material", "SX_KingstonB", "db3d041d33cfc0027f425670a191871c", 1042, false),
            A("HB_Single_Oakley.material", "SX_Springfield", "2631a4948e9323935236dccf5a5123ef", 1045, false),
            A("HB_Single_PartsUnlimited.material", "SX_Kingston", "e558637b3eef5367059c92c6a61194f7", 1053, false),
            A("HB_Single_PitPassRadio.material", "SX_KingstonB", "3e7fd2021ffaf07125973dd2204c7459", 1051, false),
            A("HB_Single_ProCircuit.material", "SX_ManchesterB", "56ec2a392ae35d2c745736eaeb400bfd", 1049, false),
            A("HB_Single_RKRacing.material", "NAT_RenneB", "ab755ca75b68b3febdcdbba58c7cd426", 1047, false),
            A("HB_Single_RaceTech.material", "FMX_BuenaVistaB", "fe56d61cdb9995d4a470243abfc9912c", 1047, false),
            A("HB_Single_Renthal.material", "FMX_BuenaVistaB", "15f3fda9896ac5a38e867af3dcca4a50", 1046, false),
            A("HB_Single_ScaryFast.material", "SX_FortDodgeB", "6b463bcd0104c7a47973dd5a7234627e", 1048, false),
            A("HB_Single_Shift.material", "SX_FortDodge", "5b352f393f52caec2ff2da97d032cd6a", 1044, true),
            A("HB_Single_Sidi.material", "SX_FortDodge", "b2c43378fb0cdde183d8a4be5f43fb71", 1043, true),
            A("HB_Single_SmithOptics.material", "FMX_Dover", "1e22afbaab5e73fc5133d6fdb6ae8b01", 1050, false),
            A("HB_Single_Spy.material", "SX_SpringfieldB", "6332920ab5e928d760e7c318073e55ff", 1042, false),
            A("HB_Single_TroyLeeDesigns.material", "SX_Kingston", "c5be85efa6d6c7aac88dd4a8dda2b099", 1053, false),
            A("HB_Single_Yoshimura.material", "SX_Manchester", "1f305e27526ec0aac91e99fb3797bdea", 1048, false),
            A("HB_Wedge_Cernics.material", "SX_FortDodge", "b2b3155d96f9f3aca0fbba0b65adce74", 1055, true),
            A("HB_Wedge_DG.material", "SX_FortDodge", "d8a0f824041a93118ee5bc705c01f7cf", 1050, true),
            A("HB_Wedge_FMF.material", "SX_FortDodgeB", "eef0c44705d70b9e8c08040191100792", 1051, false),
            A("HB_Wedge_Hiper.material", "SX_FortDodge", "f211e6e74cc32bb4bffaa9389a4e31bc", 1053, true),
            A("HB_Wedge_PitPassRadio.material", "SX_KingstonB", "cd2c9c25217121c30cd07d92c4369774", 1060, false),
            A("HB_Wedge_ProCircuit.material", "SX_ManchesterB", "ae85c362fc6c726dd14653acae8aa96e", 1058, false),
            A("HB_Wedge_ScaryFast.material", "SX_FortDodgeB", "c213c0ded9c26067e06e579bd136925b", 1057, false),
            A("HB_Wedge_Shift.material", "SX_FortDodge", "73639d1048ab6f3721b677e242624447", 1053, true),
            A("HB_Wedge_TroyLeeDesigns.material", "SX_Kingston", "87daa886a9ee0100c54fe2b540bb91a9", 1062, false),
            A("Lambert_Haybale_Test.shader", "SX_FortDodge", "a282dd117d3b8d845461e607912c628c", 6365, true),
            A("SP_Acerbis.texture", "NAT_Concord", "ce89fc877d66b029132222f8031c132f", 21992, false),     // Freestyle arenas (ux_stocklib.FMX_EXTRA)
            A("SP_AlpineStars.texture", "SX_FortDodgeB", "70d352bb3c0a15f0b7a9806fb6c2c7af", 21992, false),
            A("SP_Answer.texture", "SX_FortDodge", "99e2ed19c3d99d4d36463fb95abc4624", 21992, true),
            A("SP_Bell.texture", "SX_Kingston", "6e07073a63abe6402ddced6f85533188", 21992, false),
            A("SP_BigGun.texture", "Ctrl_AmbientAnimation", "c397910db62472655fd1a39be2279d8b", 21992, false),
            A("SP_BlingStar.texture", "FMX_BuenaVista", "362a2c3d2c407ba7cc9faddc86a9ed8f", 21992, false),
            A("SP_Blur.texture", "Ctrl_AmbientAnimation", "30b0bde98ac7cd6f22b84035093b3606", 21992, false),
            A("SP_Cernics.texture", "SX_FortDodge", "78a423965cdb94f61b152e868e38087f", 21992, true),
            A("SP_DG.texture", "SX_FortDodge", "b66e9f6bce62ab59c7a0f1df930a9e27", 21992, true),
            A("SP_DuncanRacing.texture", "Ctrl_AmbientAnimation", "6999f859a1b452351b913d6dbc832e42", 21992, false),
            A("SP_EVS.texture", "SX_Manchester", "012244b50c23af646bd14cb16587ab46", 21992, false),
            A("SP_Excel.texture", "Ctrl_AmbientAnimation", "947984b5fbedd83d1c4997589c679802", 21992, false),
            A("SP_FLY.texture", "SX_Kingston", "9b7448cee3894eaea5f7b8e23ad7c990", 21992, false),
            A("SP_FMF.texture", "SX_Kingston", "1822ad3c202d083e795d1a4686a0a307", 21992, false),
            A("SP_FactoryConnection.texture", "FMX_BuenaVista", "74b53bd6afb2aba8b0191fc191c9d9f2", 21992, false),     // Freestyle arenas (ux_stocklib.FMX_EXTRA)
            A("SP_Fox.texture", "Ctrl_AmbientAnimation", "2105d1165ecdcfec83569cd0ee3ef5fe", 21992, false),
            A("SP_Hiper.texture", "SX_FortDodge", "87f3664f8b968ec634bd90db4327cfa8", 21992, true),
            A("SP_IMSRoll.texture", "NAT_Bedford", "ab3998001ae4bc18cd9a979380631bd8", 21992, false),     // Freestyle arenas (ux_stocklib.FMX_EXTRA)
            A("SP_KTM.texture", "SX_Kingston", "7ebdb04c3749201798d3f2a63de10620", 21992, false),
            A("SP_Kenda.texture", "SX_Manchester", "54e3da94a4340c2b252ebfc00cfcb286", 21992, false),
            A("SP_Laegers.texture", "FMX_BuenaVistaB", "5097a197206aafff00ee9677c776bac4", 21992, false),
            A("SP_MSR.texture", "SX_Kingston", "ddd9cc975bae976b953b303c26d2f992", 21992, false),
            A("SP_Maxxis.texture", "Ctrl_AmbientAnimation", "fa8c1bfeaa5316eea81182e254e5ba0a", 21992, false),
            A("SP_Oakley.texture", "SX_Springfield", "c165fe1d3b7aaeeee7ba1aba4a966542", 21992, false),
            A("SP_OneIndustries.texture", "Ctrl_AmbientAnimation", "994d19cc59c32303475db858513bda22", 21992, false),
            A("SP_PartsUnlimited.texture", "SX_Kingston", "f7a0cab1b33713d60e963df08796add9", 21992, false),
            A("SP_PitPassRadio.texture", "SX_Kingston", "82ccdd1484d2da7f0f8eb2a8d5b43e53", 21992, false),
            A("SP_ProCircuit.texture", "SX_ManchesterB", "0603ef2d3731f3f2724282c9b45de44c", 21992, false),
            A("SP_RKRacing.texture", "Ctrl_AmbientAnimation", "2be7fa6d3c450d531e83902272286da4", 21992, false),
            A("SP_RaceTech.texture", "SX_FortDodge", "acf58d9f160e074f909832be62c35826", 21992, true),
            A("SP_Renthal.texture", "FMX_BuenaVistaB", "c1c5cda27752622d7cb01ced01d76ae4", 21992, false),
            A("SP_ScaryFast.texture", "SX_FortDodgeB", "265dc682657f10417f02c1e1dfe9c3c6", 21992, false),
            A("SP_Scott.texture", "FMX_BuenaVistaB", "c92018f1fa2bde9c6f13bc74163bab17", 21992, false),     // Freestyle arenas (ux_stocklib.FMX_EXTRA)
            A("SP_Shift.texture", "SX_FortDodge", "fbaa7373d7bd4818a5ddb6470002d812", 21992, true),
            A("SP_Sidi.texture", "SX_FortDodge", "c9db972d39600c25eb74a42025d35249", 21992, true),
            A("SP_SixSixOne.texture", "SX_Kingston", "7b2f66a5c7985b19b9824078a917b989", 21992, false),     // Minimoto (ux_stocklib.MM_EXTRA)
            A("SP_SmithOptics.texture", "FMX_Dover", "0adc69bdcdf29c7ffc02929e79a12b4b", 21992, false),
            A("SP_Spy.texture", "SX_SpringfieldB", "0505bcd0139638c842c0e09df442f67a", 21992, false),
            A("SP_TroyLeeDesigns.texture", "SX_Kingston", "d899e09d814e255cd7eb5bb2fcddbf90", 21992, false),
            A("SP_Yoshimura.texture", "SX_Manchester", "c164e6d1244b6d400d74832594d6bc05", 21992, false),
            A("Template_Trackside_Haybale_FNL.shader", "SX_FortDodge", "4ea72dde1e71a2c8eab2f13039f0fd92", 6365, true),
            A("Water.shader", "FR_Alpine", "729778b88ebc4c47f20d51338e82bd21", 36974, false),
            A("WaterFoam.texture", "FR_Alpine", "85ac67c5fa6c19d3db3e38c7ec1fbcb1", 2856, false),
            A("WaterPressure.texture", "FR_Gibson", "f0bae62b6519a9b264a67ad88f986f35", 2856, false),
            A("Water_Wake.shader", "FR_Gibson", "41180df4c528a8b791b27ff7bbb700d9", 1812, false),
        };
        /// templates/stock-lib/index.json 'prototypes'.
        public static readonly UxStockPrototype[] Prototypes =
        {
            new UxStockPrototype("MassiveAdEntityRSData", "prototypes/MassiveAdEntityRSData.bxml", "SX_Kingston", "SX_KingstonBase", 126, 16, 3457837, "842c9957c9803546cd389377bf5b3987", 793),
            new UxStockPrototype("WaterSystemRSData", "prototypes/WaterSystemRSData.bxml", "SX_DLC01_FreightYard", "SX_DLC01_FreightYardBase", 101, 9, 6746999, "57e58954f7230dea4ffa31b6ab533f78", 588),
        };
        /// ux_stocklib.BASE_GAPS (= ux_build_track.BASE_GAPS): template textures the converted materials name that the DE sketch carried
        /// and SX_FortDodge lacks; the builder adds them from here (Get) when a material references one and the base has no such name.
        public static readonly (string name, string type)[] BASE_GAPS = { ("Editor_Default_Gray", "texture"), ("Editor_Default_BlackAlpha", "texture") };
        /// ux_stocklib.WATER_KIT: the assets the core / client look up by name for a WaterSystemRSData (UxWater.KIT is the build order).
        public static readonly (string name, string type)[] WATER_KIT = { ("Water_Wake", "shader"), ("WaterPressure", "texture"), ("WaterFoam", "texture"), ("Water", "shader") };
        /// ux_stocklib.SPLASH_KIT (water-look round): the EC water spray takes the bike TireInWater emitters, their atlas tiles and
        /// partattr records from FR_Gibson's particle kit (UxWater.Splash).
        public static readonly (string name, string type)[] SPLASH_KIT = { ("FR_Gibson_Particles", "particle"), ("FR_Gibson_Particles", "partattr"), ("FR_Gibson_Particles", "texture") };
        /// ux_stocklib.FD_WRAPS: SX_FortDodge's own bale wraps (--tuff-wraps fortdodge).
        public static readonly string[] FD_WRAPS = { "Answer", "Cernics", "DG", "Hiper", "Shift", "Sidi" };
        static Dictionary<string, UxStockAsset> s_assets;
        static Dictionary<string, UxStockAsset> AssetIndex()
        {
            var d = s_assets;
            if (d == null) { d = new Dictionary<string, UxStockAsset>(StringComparer.Ordinal); foreach (var a in Assets) d[a.key] = a; s_assets = d; }
            return d;
        }

        /// ux_stocklib.has(name, typ).
        public static bool Has(string name, string typ) => AssetIndex().ContainsKey(name + "." + typ);
        /// ux_stocklib.record(name, typ): null when the library does not list it.
        public static UxStockAsset Record(string name, string typ) => AssetIndex().TryGetValue(name + "." + typ, out var r) ? r : null;

        /// The logical bytes of an asset of a stock level (ux_stocklib._read): null when the level has no such asset.
        public byte[] ReadStock(string level, string name, string typ)
        {
            var ts = library.OpenLevel(level);
            var a = ts.TryGet(name, typ);
            return a == null ? null : ts.ReadLogical(a);
        }

        byte[] Cached(string key, string md5, string what, Func<byte[]> make)
        {
            lock (_gate) if (_cache.TryGetValue(key, out var hit)) return hit;
            byte[] b = make();
            if (b == null) throw new UxException(what + " is not in the Reflex library (" + library.Describe + ")");
            string got = UxBin.Md5(b);
            if (got != md5) throw new UxException(what + " does not match its pinned md5 " + md5 + " (got " + got + ": another Reflex version?)");
            lock (_gate) _cache[key] = b;
            return b;
        }

        /// ux_stocklib.get(name, typ): the bytes of a library asset (md5 checked against the pin), or null when the library does not
        /// list it. The array is shared: do not modify it.
        public byte[] Get(string name, string typ)
        {
            var rec = Record(name, typ);
            if (rec == null) return null;
            return Cached("stock-lib/assets/" + rec.key, rec.md5, "stock asset " + rec.key + " (" + rec.source + ")", () => ReadStock(rec.source, name, typ));
        }

        static UxStockPrototype Proto(UxStockPrototype[] table, string rsdata)
        {
            foreach (var p in table) if (p.rsdata == rsdata) return p;
            return null;
        }

        /// ux_stocklib._prototype_bytes / build_lib.build_prototypes: the one-entity SceneChunk of a stock entity, with the stock attr
        /// flags (encode_tree, apply_flags, to_bytes: zlib level 6).
        public byte[] PrototypeBytes(UxStockPrototype p, string group = "stock-lib")
            => Cached(group + "/" + p.file, p.md5, "prototype " + p.rsdata + " (" + p.SourceScene + ")", () => BuildPrototype(library.ReadLevelFile(p.level, "scene"), p));

        /// The prototype bytes from a stock scene file.
        public static byte[] BuildPrototype(byte[] scene, UxStockPrototype p)
        {
            List<byte[]> raw = null;
            foreach (var kv in UxPcScene.RawChunks(scene)) if (kv.Key == p.subScene) { raw = kv.Value; break; }
            if (raw == null || p.chunk >= raw.Count) throw new UxException("no chunk " + p.chunk + " of " + p.subScene + " in " + p.SourceScene);
            var sb = new UxBxml(raw[p.chunk]);
            UxBin.Require(p.indexInChunk < sb.ChildCount(0), p.SourceScene + ": chunk " + p.chunk + " has no entity " + p.indexInChunk);
            int entNode = sb.FirstChild(0) + p.indexInChunk;
            object eid = sb.Attr(entNode, "entityId");
            UxBin.Require(eid is long l && l == p.entityId, p.SourceScene + ": entity " + p.entityId + " is not at chunk " + p.chunk + " / " + p.indexInChunk);
            var flags = new List<long[]> { new long[0] };
            flags.AddRange(sb.FlagsWalk(entNode));
            var root = new UxElem("SceneChunk", null, 0, 0, null, new List<UxElem> { sb.ToTree(entNode) });
            var b2 = new UxBxml(UxBxml.EncodeTree(root, sb.version, sb.unknown));
            b2.ApplyFlags(flags);
            return b2.ToBytes();
        }

        static (UxElem entity, long version, long unknown) PrototypeElem(byte[] data, string file)
        {
            var b = new UxBxml(data);
            var chunk = b.ToTree();
            if (chunk.tag != "SceneChunk" || chunk.children.Count != 1) throw new UxException(file + " is not a one-entity SceneChunk");
            return (chunk.children[0], b.version, b.unknown);
        }

        /// ux_stocklib.prototype(rsdata): (Entity Elem, (version, unknown)) of a stock-lib prototype (md5 checked). A fresh tree each call.
        public (UxElem entity, long version, long unknown) Prototype(string rsdata)
        {
            var p = Proto(Prototypes, rsdata) ?? throw new UxException("no stock-lib prototype " + rsdata);
            return PrototypeElem(PrototypeBytes(p), p.file);
        }

        // ------------------------------------------------------------------------------------------ reflex-lib (ux_common.lib_*)
        /// templates/reflex-lib/index.json 'templates' (the 8 material templates).
        public static readonly UxLibTemplate[] LibTemplates =
        {
            new UxLibTemplate("Template_Diffuse", "SX_Kingston", "SX_KingstonBase", "AV_AmbientTire_01", "SX_KingstonBase", "68fe1a23287c72fbfbde3abddbec2f4a", 5949, "9913e37bdeaf249a09833f6b63841603", 884, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 1,
                new[] { "AV_AmbientTire_01" }, new[] { "diffuse_texture" },
                new[] { "s0", "usr_sampler_diffuse_texture", "s12", "samplerVolumeShadowMap", "s13", "samplerAmbientDirectionalLightColorRef", "s15", "samplerDynamicShadowMap" }),
            new UxLibTemplate("Template_Diffuse_AO", "SX_Kingston", "SX_KingstonBase", "GL_BoomCamCart", "SX_KingstonBase", "752321b364a482a849b6ccc8f9ef9a6d", 6101, "2d3d3cf62d6be38f999b399a465ae85f", 1168, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 2,
                new[] { "GL_BoomCartCam_AO", "GL_BoomCamCart" }, new[] { "ao_texture", "diffuse_texture" },
                new[] { "s0", "usr_sampler_ao_texture", "s1", "usr_sampler_diffuse_texture", "s12", "samplerVolumeShadowMap", "s13", "samplerAmbientDirectionalLightColorRef", "s15", "samplerDynamicShadowMap" }),
            new UxLibTemplate("Template_Diffuse_Alpha", "SX_Kingston", "SX_KingstonBase", "UI_StartCard_01", "SX_KingstonBase", "334b60c92b5210e4e231898d0cf55d77", 5913, "abb7b2d2846dd03327da6a6727e0b29e", 1081, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 2,
                new[] { "UI_StartGateCards_01", "UI_StartGateCards_01" }, new[] { "diffuse_texture", "alpha_texture" },
                new[] { "s0", "usr_sampler_diffuse_texture", "s1", "usr_sampler_alpha_texture", "s12", "samplerVolumeShadowMap", "s13", "samplerAmbientDirectionalLightColorRef", "s15", "samplerDynamicShadowMap" }),
            new UxLibTemplate("Template_Diffuse_Nrm", "SX_Kingston", "SX_KingstonBase", "MX10_256_128_A_LargeBanner", "SX_KingstonBase", "5df7d9d80159cd1d65ebb7a8e2691a29", 6133, "13edcd3410e318c9240c478e7c95e07c", 1044, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 2,
                new[] { "MX10_256X128_A", "Banner2_NM" }, new[] { "diffuse_texture", "normal_texture" },
                new[] { "s0", "usr_sampler_diffuse_texture", "s1", "usr_sampler_normal_texture", "s12", "samplerVolumeShadowMap", "s13", "samplerAmbientDirectionalLightColorRef", "s15", "samplerDynamicShadowMap" }),
            new UxLibTemplate("Template_Eco_Frond", "FR_Gibson", "FR_GibsonBase", "Eco_HoneyMesquite_Fr", "FR_GibsonBase", "fbc1414d4348f0e6b08a5751a8adb3b7", 19242, "149c3fb6f0f42570cf1b6468b0bf64ff", 1378, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 1,
                new[] { "FR_GI_HoneyMesquite_CM_Diffuse" }, new[] { "diffuse_texture" },
                new[] { "s0", "usr_sampler_diffuse_texture", "s12", "samplerVolumeShadowMap", "s13", "samplerAmbientDirectionalLightColorRef" }),
            new UxLibTemplate("Template_Prelit", "SX_Kingston", "SX_KingstonBase", "SX_KI_B_ConcreteRim_E", "SX_KingstonBase", "fa70b1c44524fd94286b3bf68fd8ddb4", 4393, "4531398543f8f98f7efec0e987d977d1", 1049, "VertexShader+PixelShader", "StaticVertexShader", "SkinnedVertexShader", "SharedPixelShader", 2,
                new[] { "SX_KI_E_Tier_LM", "SX_KI_ConcreteRim01" }, new[] { "prelit_light_ao_texture", "diffuse_base_texture" },
                new[] { "s0", "usr_sampler_prelit_light_ao_texture", "s1", "usr_sampler_diffuse_base_texture" }),
            new UxLibTemplate("Water", "FR_Alpine", "FR_AlpineBase", "FR_AL_RiverMain", "FR_AlpineBase", "729778b88ebc4c47f20d51338e82bd21", 36974, "1fe0bb92a6f3f48c4e5faefea8045bc0", 2137, "Shaders@asset", "vs_river_lod0", null, "ps_river_lod0", 1,
                new[] { "WaterFoam" }, new[] { "foam" },
                new[] { "s0", "usr_sampler_foam", "s5", "OceanWaveSampler", "s6", "WakeSampler", "s7", "RefractionSampler", "s8", "RefractionDepthSampler", "s14", "ReflectionSampler" }),
            new UxLibTemplate("crowd", "SX_Kingston", "SX_KingstonBase", "MaleCrowd02_Static_Sit_G", "SX_KingstonBase", "5e70a6ef7e0fa63955738b1d07c2e4cf", 6275, "6a0705a8fb5c7dad24308047742f3a6e", 712, "Shaders@asset", "StaticVertexShader", null, "CrowdPixelShader", 2,
                new[] { "MaleCrowd02_Static_Sit_G", "EmptyStadiumChair_G" }, new[] { "BaseMap", "EmptyChair" },
                new[] { "s0", "usr_sampler_BaseMap", "s1", "usr_sampler_EmptyChair" }),
        };
        /// templates/reflex-lib/index.json 'prototypes' (SX_Kingston entities).
        public static readonly UxStockPrototype[] LibPrototypes =
        {
            new UxStockPrototype("BillboardCrowdRSData", "prototypes/BillboardCrowdRSData.bxml", "SX_Kingston", "SX_KingstonBase", 72, 2, 1046859, "29bc72d71e871e9db302dabeb4316e17", 898),
            new UxStockPrototype("CollisionRSData", "prototypes/CollisionRSData.bxml", "SX_Kingston", "SX_KingstonBase", 119, 7, 1177132, "baaae34306214f3ff405d0492686e4a1", 1012),
            new UxStockPrototype("DistanceFogRSData", "prototypes/DistanceFogRSData.bxml", "SX_Kingston", "SX_KingstonBase", 87, 5, 1047680, "196b898acf7822f2fa8a22af084325e4", 753),
            new UxStockPrototype("ModelRSData", "prototypes/ModelRSData.bxml", "SX_Kingston", "SX_KingstonBase", 0, 8, 1749884, "14e566bba92ff5594286512240582368", 797),
            new UxStockPrototype("PhysicsRSData", "prototypes/PhysicsRSData.bxml", "SX_Kingston", "SX_KingstonBase", 2, 19, 1057504, "fb4cba7ca71070ec21e622b23f7f2821", 1303),
            new UxStockPrototype("PopDownBaleRSData", "prototypes/PopDownBaleRSData.bxml", "SX_Kingston", "SX_KingstonBase", 70, 14, 1179658, "22c280000f20f56abc21126282d793e9", 1347),
            new UxStockPrototype("PopUpBaleRSData", "prototypes/PopUpBaleRSData.bxml", "SX_Kingston", "SX_KingstonBase", 71, 1, 1179674, "80c30db5503a751df634ecdb3b8cc364", 1346),
            new UxStockPrototype("StartEntityFreeRideRSData", "prototypes/StartEntityFreeRideRSData.bxml", "SX_Kingston", "SX_KingstonFreeRide", 0, 0, 1062018, "9a0b3fed1391b3412652d7fc78c68469", 512),
            new UxStockPrototype("TriggerRSData", "prototypes/TriggerRSData.bxml", "SX_Kingston", "SX_KingstonBase", 72, 0, 4314679, "dc431bdfb06fb38f63dd1f29d8162af8", 677),
        };

        /// lib_index()['templates'][T]; null when there is no such template.
        public static UxLibTemplate LibTemplate(string template)
        {
            foreach (var t in LibTemplates) if (t.name == template) return t;
            return null;
        }
        static UxLibTemplate NeedTemplate(string template) => LibTemplate(template) ?? throw new UxException("no reflex-lib template " + template);

        /// lib_shader_bytes(template): the stock .shader asset (D3D9 container) of a material template, e.g. 'Template_Prelit'.
        public byte[] LibShaderBytes(string template)
        {
            var t = NeedTemplate(template);
            return Cached("reflex-lib/shaders/" + t.name + ".shader", t.shaderMd5, "stock shader " + t.name + " (" + t.sourceSet + ")",
                () => ReadStock(t.sourceSet, t.name, "shader"));
        }
        /// lib_material_bytes(template): the stock exemplar .material (BXML) of a material template.
        public byte[] LibMaterialBytes(string template)
        {
            var t = NeedTemplate(template);
            return Cached("reflex-lib/materials/" + t.name + ".material", t.exemplarMd5, "stock material " + t.exemplar + " (" + t.sourceSet + ")", () =>
            {
                var ts = library.OpenLevel(t.sourceSet);
                foreach (var a in ts.assets) if (a.type == "material" && a.name == t.exemplar && a.PackageName == t.exemplarPackage) return ts.ReadLogical(a);
                return ReadStock(t.sourceSet, t.exemplar, "material");
            });
        }
        /// The file bytes of prototypes/<rsdata>.bxml of the reflex-lib.
        public byte[] LibPrototypeBytes(string rsdata)
        {
            var p = Proto(LibPrototypes, rsdata) ?? throw new UxException("no reflex-lib prototype " + rsdata);
            return PrototypeBytes(p, "reflex-lib");
        }
        /// lib_prototype(rsdata): (Entity Elem, (version, unknown)) of the one stock Entity of the RSData type. A fresh tree each call.
        public (UxElem entity, long version, long unknown) LibPrototype(string rsdata) => PrototypeElem(LibPrototypeBytes(rsdata), "prototypes/" + rsdata + ".bxml");

        // ------------------------------------------------------------------------------------------ the clone base (ux_base)
        public const string DEFAULT_STOCK_LEVEL = "SX_FortDodge";
        public const long BASE_SYNTH_ID = 18000001;
        /// STOCK_PINS[level][kind]: the md5 of the stock level's four files (null when the level has no pins).
        public static string StockPin(string level, string kind)
        {
            if (level != "SX_FortDodge") return null;
            switch (kind)
            {
                case "database": return "55d308da8969d938b83838ff8893cb61";
                case "level": return "139f1a73845221cfe40c924499add011";
                case "package": return "ed0caf84e0df49fa3f810117bc3901ae";
                case "scene": return "d09209839b511b7c6387018eb722e7d3";
            }
            return null;
        }
        /// One of the base's whole files: 'database', 'level' or 'scene' (md5 checked against STOCK_PINS, as ensure_stock_copy). The
        /// package is not read whole: BaseSet gives its assets.
        public byte[] BaseFile(string kind, string level = DEFAULT_STOCK_LEVEL)
        {
            UxBin.Require(kind == "database" || kind == "level" || kind == "scene", "BaseFile: " + kind);
            string pin = StockPin(level, kind) ?? throw new UxException("no md5 pins for stock level " + level);
            return Cached("base/" + level + ".dx9." + kind, pin, "stock level file " + level + ".dx9." + kind, () => library.HasLevel(level) ? library.ReadLevelFile(level, kind) : null);
        }
        /// pkg.TrackSet of the base (templates/stock-<level>/): assets by (name, type), logical bytes on demand. In an extracted library
        /// (the client's game files) the stored blobs do not exist; UxTrackSet.Blob then re-encodes (stored chunks).
        public UxTrackSet BaseSet(string level = DEFAULT_STOCK_LEVEL)
        {
            BaseFile("database", level);
            return library.OpenLevel(level);
        }

        // ------------------------------------------------------------------------------------------ the reference's folders
        /// Writes templates/stock-lib (assets/, prototypes/) and templates/reflex-lib (shaders/*.shader, materials/, prototypes/) as the
        /// reference holds them, into `folder` (tests; the converter itself reads through the methods above). Returns the relative paths.
        public List<string> WriteReferenceFolders(string folder)
        {
            var done = new List<string>();
            void Put(string rel, byte[] data)
            {
                string p = Path.Combine(folder, rel.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(p));
                File.WriteAllBytes(p, data); done.Add(rel);
            }
            foreach (var a in Assets) Put("stock-lib/assets/" + a.key, Get(a.name, a.type));
            foreach (var p in Prototypes) Put("stock-lib/" + p.file, PrototypeBytes(p));
            foreach (var t in LibTemplates)
            {
                Put("reflex-lib/shaders/" + t.name + ".shader", LibShaderBytes(t.name));
                Put("reflex-lib/materials/" + t.name + ".material", LibMaterialBytes(t.name));
            }
            foreach (var p in LibPrototypes) Put("reflex-lib/" + p.file, PrototypeBytes(p, "reflex-lib"));
            return done;
        }
    }
}

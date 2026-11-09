// The original object shading (vehicles, rider gear and body, characters, crowds) for Reflex/Vehicle (Shaders/ReflexVehicle.shader):
// _re/notes/object-shading.md. A material of the game names a shader asset (PixelShader asset = the material's "shader", with
// StaticVertexShader / SkinnedVertexShader / SharedPixelShader programs); Shaders/ReflexObjectPrograms.hlsl ports each static vertex
// program and each pixel program of the object shader assets, and Programs below is its table (the program pair's id = _Program).
// A material is bound as the game binds it:
//   program     the shader asset's pixel program by its bytecode (FNV-1a 64; the name for a program the table has under another name)
//   textures    the material's Texture entries in order = sampler registers s0.. (cubes where the program declares dcl_cube), with
//               the entry's XenonSamplerStates (address U / V, min / mag / mip filter, LOD bias, max anisotropy); raw values (no sRGB:
//               the original device samples without conversion), G16R16F normal maps as RGHalf, FP16 cubes as RGBAHalf
//   constants   every float constant of the two programs' constant tables that is not one of the renderer's globals: the
//               material's Parameter of that name over the program's CTAB default (the game's ConstantTable SetDefaults, then the
//               material's values); run-time constants the game sets per object (usr_mud_*, gTextureOffset, usr_fade_value,
//               usr_damage*) are set on the material by their owners (ReflexMud, SetRuntime)
//   states      CULLMODE, ZWRITEENABLE, ALPHATESTENABLE / ALPHAREF / ALPHAFUNC, ALPHABLENDENABLE / SRCBLEND / DESTBLEND
// Level globals: ApplyLevelGlobals (the reflection cube s14, the dynamic shadow source); the rest is ReflexScenery.ApplyShaderGlobals.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public static class ReflexObjectShading
    {
        /// The program table of Shaders/ReflexObjectPrograms.hlsl (index = _Program): shader asset, FNV-1a 64 of its pixel program
        /// and of its static vertex program. Generated with the programs (objshade tools/gen.py).
        public static readonly (string name, ulong ps, ulong vs)[] Programs =
        {
            ("Boots_FNL", 0x505c5f508d02b103UL, 0x7e85da89811e2ce7UL),
            ("Boots_UI__FNL", 0x8bc9d59e7d68fd79UL, 0x7e85da89811e2ce7UL),
            ("Chestprt_bck_FNL", 0x5ae24b389c1f81ecUL, 0x68bde8f60ac3d69fUL),
            ("Chestprt_cloth_FNL", 0x95923baa9b25efc2UL, 0x7e85da89811e2ce7UL),
            ("Chestprt_frnt_FNL", 0x604569f5f1699a55UL, 0x7e85da89811e2ce7UL),
            ("Chestprt_frnt_Opaque_FNL", 0x67baee03b29c0794UL, 0x7e85da89811e2ce7UL),
            ("Chestprt_plst_FNL", 0x721c6c5adf93a4f5UL, 0x7e85da89811e2ce7UL),
            ("Editor_DiffuseColor", 0xbebba54b28a5c49cUL, 0x23b6373966dced20UL),
            ("Editor_DiffuseColor", 0x0459d83bf82be054UL, 0x112ae279d7ea0193UL),
            ("Editor_DiffuseColor", 0x4d076418fd603e72UL, 0x5e17a8c9437edec7UL),
            ("Goggle_FNL", 0x4f6ae8dfbdcea560UL, 0x68bde8f60ac3d69fUL),
            ("Goggle_UI_FNL", 0x641c33203d0ca36bUL, 0x7e85da89811e2ce7UL),
            ("Helmet_FNL", 0xf8a0d5dda2233cccUL, 0xa9ec70b9f4146208UL),
            ("Helmet_UI_FNL", 0x33c6222cccc19547UL, 0x7e85da89811e2ce7UL),
            ("Helmet_cloth_FNL", 0xc80f5be33ff1e24aUL, 0x68bde8f60ac3d69fUL),
            ("Helmet_matte_FNL", 0xed99082a08dada11UL, 0xa9ec70b9f4146208UL),
            ("Helmet_matte_UI_FNL", 0x16430e6cbf677b8aUL, 0x7e85da89811e2ce7UL),
            ("Jersey_FNL", 0x7c06941706a6d37bUL, 0x464960e5adb8573bUL),
            ("Lens_Clr_FNL", 0xc224a055ae57bbd2UL, 0xb5b490e67339e8bdUL),
            ("Lens_Opaque_FNL", 0x7b3572007772ce28UL, 0xb5b490e67339e8bdUL),
            ("MX_Hguard", 0xc9e0846e06cfaf89UL, 0x23b6373966dced20UL),
            ("NckBrace_plstc_FNL", 0x112a151d45821bceUL, 0x7e85da89811e2ce7UL),
            ("Pro401_Alpha", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro401_Engine", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro401_Frame", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro401_Tires", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro401_Wheels", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro402_Engine", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro402_Frame", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro402_Tires", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("Pro402_Wheels", 0x57c54aab17408feeUL, 0x23b6373966dced20UL),
            ("SB_Flywheels", 0x275e49c45da1974fUL, 0x4ab922850b984b36UL),
            ("Skin_FNL", 0x90075c29128a29efUL, 0xa9ec70b9f4146208UL),
            ("Template_ATV_Bumper_Alpha_FNL", 0xfe4eae78a00b70deUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Bumper_Diffse_Alpha_UI_FNL", 0x5a6705a6eb6ec1b4UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Bumper_FNL", 0xa0ed41fe3487a01bUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Frame_FNL", 0x1531b84b3fbfca77UL, 0xa9ec70b9f4146208UL),
            ("Template_ATV_Handgrd_FNL", 0xeea67ed15b746309UL, 0xa9ec70b9f4146208UL),
            ("Template_ATV_Handgrd_UI_FNL", 0xa9656df6bd562752UL, 0xa9ec70b9f4146208UL),
            ("Template_ATV_MX_AlphaMtl_UI_FNL", 0x8d9cb1838f6526cbUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_MX_Engine", 0x81fd09b6201644f2UL, 0x7e85da89811e2ce7UL),
            ("Template_ATV_MX_Engine_FNL", 0x258982ea1eb5a168UL, 0xa9ec70b9f4146208UL),
            ("Template_ATV_MX_Engine_FNL", 0x0645cf89a81e3830UL, 0xfb48d954b325cfdbUL),
            ("Template_ATV_MX_HBars", 0x90ad2615320e74f7UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_MX_HBars_Pad", 0x8daffb763b912477UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_MX_Hbars_FNL", 0x9e99363b0ad7646aUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_MX_Plstc", 0x702be921ffffbc55UL, 0x464960e5adb8573bUL),
            ("Template_ATV_NerfSpec_Alpha_FNL", 0x905f18f0fad139a1UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_NerfSpec_FNL", 0x91449225d93a0ed9UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Nerf_Alpha_FNL", 0xc847adf2738ee310UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Nerf_FNL", 0x8aa03256200b1252UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Plastic_FNL", 0x31ed205f8ec34f82UL, 0x8b95e934fe796e0eUL),
            ("Template_ATV_Shock_Metal", 0x0d517d3b43bcccd8UL, 0x7e85da89811e2ce7UL),
            ("Template_ATV_Spring_FNL", 0x647135b07b36716cUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_Spring_UI_FNL", 0x8479b2dca3e64525UL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_shock_body_FNL", 0xc80f5be33ff1e24aUL, 0x68bde8f60ac3d69fUL),
            ("Template_ATV_shock_body_UI_FNL", 0x9d9e71e5695af313UL, 0x68bde8f60ac3d69fUL),
            ("Template_Aarms_FNL", 0x24f236cf1ef68792UL, 0x68bde8f60ac3d69fUL),
            ("Template_Body_Boots_NM", 0x4da6667247f91292UL, 0x56d4d64212e63f6fUL),
            ("Template_Body_Boots_NM", 0xe06410651295adb8UL, 0x7e85da89811e2ce7UL),
            ("Template_Body_Earrings", 0x5b6b81085323586dUL, 0xa4d0455a39df3ae6UL),
            ("Template_Body_Earrings", 0xacc4a1514ac5b532UL, 0xb5b490e67339e8bdUL),
            ("Template_Body_Hair", 0x7fc1280e5f42bcc4UL, 0xcca1f98c7c4a136bUL),
            ("Template_Body_Hair", 0x97372f110e24a256UL, 0xa9ec70b9f4146208UL),
            ("Template_Body_Head", 0x3ed06d6a0afe63c8UL, 0xcd3548d96fdee092UL),
            ("Template_Body_Head", 0x5c5fd99e1402de6cUL, 0xa9ec70b9f4146208UL),
            ("Template_Body_Jersey", 0xf5b8ef5e7b56ba80UL, 0x03c9eb8282ec0d33UL),
            ("Template_Body_Jersey", 0x693052a42cafad8fUL, 0x7e85da89811e2ce7UL),
            ("Template_Body_JerseyDyn_Cool", 0x2319126f55690b8dUL, 0xcfbd086637d2a25cUL),
            ("Template_Body_JerseyDyn_Cool", 0x7a5313ff66f2d4ecUL, 0x7e85da89811e2ce7UL),
            ("Template_Body_JerseyDyn_Warm", 0x986e30d1ada67e35UL, 0xcfbd086637d2a25cUL),
            ("Template_Body_JerseyDyn_Warm", 0xb16d5af09983a831UL, 0x7e85da89811e2ce7UL),
            ("Template_Body_Pants", 0xf64744792befc40fUL, 0x56d4d64212e63f6fUL),
            ("Template_Body_Pants", 0xce7337d7c08c1985UL, 0x7e85da89811e2ce7UL),
            ("Template_Body_Skin", 0xd3b2f429447209b6UL, 0x7a028ea9e3a6a8a8UL),
            ("Template_Body_Skin", 0x9057a7420ff2c79dUL, 0x7e85da89811e2ce7UL),
            ("Template_Body_Skin_Nylon", 0x7d0aefabab397153UL, 0xecdb30f27077bfcdUL),
            ("Template_Body_Skin_Nylon", 0xffac3085a1d057b9UL, 0x7e85da89811e2ce7UL),
            ("Template_Body_Teeth", 0xf22f57a1f807e58dUL, 0xcca1f98c7c4a136bUL),
            ("Template_Body_Teeth", 0xd9a47fc047684ae3UL, 0xa9ec70b9f4146208UL),
            ("Template_Chestprt_frnt", 0xd2874e1df3aacbc7UL, 0x7e85da89811e2ce7UL),
            ("Template_Chestprt_plst", 0xd2874e1df3aacbc7UL, 0x7e85da89811e2ce7UL),
            ("Template_Crowd_Cloth", 0x36756ee381d57029UL, 0xb9808312d966935dUL),
            ("Template_Crowd_Cloth", 0xd779cfc3feec6587UL, 0x68bde8f60ac3d69fUL),
            ("Template_Crowd_Cloth", 0x85a4c2523ee9a697UL, 0x0be10791318b2be1UL),
            ("Template_Crowd_Cloth_Instanced", 0x363ada67d5ef0712UL, 0xa44a3b5433161d23UL),
            ("Template_Crowd_Cloth_Instanced", 0x018c184e41beb076UL, 0xed8eda87e3d21326UL),
            ("Template_Crowd_Props", 0x45cdd80c21907d9aUL, 0xb9808312d966935dUL),
            ("Template_Crowd_Props", 0x49278907ed962ca2UL, 0x68bde8f60ac3d69fUL),
            ("Template_Crowd_Props", 0x85f92b05abb15c44UL, 0x0be10791318b2be1UL),
            ("Template_Crowd_Props_Instanced", 0x4964c28e1d669807UL, 0xa44a3b5433161d23UL),
            ("Template_Crowd_Props_Instanced", 0xd529f9e40e45d00aUL, 0xee7d77b7a1ec977bUL),
            ("Template_Crowd_Skin", 0x7e995f8d4358ece9UL, 0xb9808312d966935dUL),
            ("Template_Crowd_Skin", 0xb045985d7b6eba9bUL, 0x0be10791318b2be1UL),
            ("Template_Crowd_Skin", 0x61930fdfa62cb64bUL, 0x68bde8f60ac3d69fUL),
            ("Template_Crowd_Skin_Instanced", 0x4e25f5d761b711afUL, 0xa44a3b5433161d23UL),
            ("Template_Crowd_Skin_Instanced", 0x02877452fca52e03UL, 0x18dca5ea2e72608aUL),
            ("Template_Diffuse", 0x9e99363b0ad7646aUL, 0x68bde8f60ac3d69fUL),
            ("Template_Diffuse", 0xb67dd6d5678b9fd0UL, 0x0715d78402076373UL),
            ("Template_Diffuse", 0xbd2414064d0a6b1eUL, 0x0a2667ecc128729fUL),
            ("Template_Helmet_GStrap", 0xa51c828b87ef56c1UL, 0x7e85da89811e2ce7UL),
            ("Template_Hndlebrs_BrshMtl_UI_FNL", 0x6fc8d066d99d2ec9UL, 0x68bde8f60ac3d69fUL),
            ("Template_Hndlebrs_Spec_UI_FNL", 0x53a393838b650dcaUL, 0x68bde8f60ac3d69fUL),
            ("Template_MX_ATV_Exhst_Chr_FNL", 0xe58c7e393332d8e3UL, 0xa9ec70b9f4146208UL),
            ("Template_MX_ATV_Exhst_Mtl_FNL", 0x8b45a6b2bb05425fUL, 0x68bde8f60ac3d69fUL),
            ("Template_MX_ATV_UI_Exhst_Chr_FNL", 0x28fbc2dcc081c8c6UL, 0xa9ec70b9f4146208UL),
            ("Template_MX_ATV_UI_Exhst_Mtl_FNL", 0x8b45a6b2bb05425fUL, 0x68bde8f60ac3d69fUL),
            ("Template_MX_Frame_FNL", 0x93399be9f166cc0dUL, 0x8b95e934fe796e0eUL),
            ("Template_MX_Frame_UI_FNL", 0xb7d055ea985bc0a0UL, 0x8b95e934fe796e0eUL),
            ("Template_MX_HUB", 0x900157c8584d2534UL, 0x1bfe2af78b29d2bfUL),
            ("Template_MX_Wheel_Fnl", 0xc9dfff650f5d9f40UL, 0xa9ec70b9f4146208UL),
            ("Template_MachineBodyPntCndy_UI_FNL", 0x38a8d150e7352d1cUL, 0x047c5872ed6a9533UL),
            ("Template_MachineBodyPntCndy_UI_FNL", 0x3fe491fb00b3b8aaUL, 0x8c40cf863855e45fUL),
            ("Template_MachineBodyPntCndy_UI_FNL", 0xd44f9c3097296f81UL, 0xc97b57dba68c73b1UL),
            ("Template_MachineBody_Damge_FNL", 0x2216f858b2818c88UL, 0xbf90cb5565c51d75UL),
            ("Template_MachineBody_Damge_FbrOnly_FNL", 0x680ac41d67be7a06UL, 0xbf90cb5565c51d75UL),
            ("Template_MachineBody_FNL", 0x9f75822bf45dd960UL, 0xc97b57dba68c73b1UL),
            ("Template_Machines_Body", 0xcbdd22cabebd9e4eUL, 0x7e85da89811e2ce7UL),
            ("Template_Machines_Body_DMG", 0x01628d278b2ee2f0UL, 0x7e85da89811e2ce7UL),
            ("Template_Machines_DifSpecAlpha", 0x9bae54d14262f25eUL, 0x68bde8f60ac3d69fUL),
            ("Template_Machines_Engine", 0x7974bbbbe508a53cUL, 0x7e85da89811e2ce7UL),
            ("Template_Machines_Frame", 0x3cadc73ebcdcbb28UL, 0x7e85da89811e2ce7UL),
            ("Template_Machines_Frame_FNL", 0xf34948ee66c62959UL, 0x68bde8f60ac3d69fUL),
            ("Template_Machines_Spring", 0x043db3ae2f762f45UL, 0x68bde8f60ac3d69fUL),
            ("Template_Machines_Tube_FNL", 0xd3595b4a302429b0UL, 0x68bde8f60ac3d69fUL),
            ("Template_Vehicle_Plstc_Fnl", 0xcc7aa59d8ba4d353UL, 0x8b95e934fe796e0eUL),
            ("Template_Vehicle_Seat", 0x69724d92a90e0692UL, 0x7e85da89811e2ce7UL),
            ("Template_Vehicle_Seat_FNL", 0x7c96178a7c0acc00UL, 0xa9ec70b9f4146208UL),
            ("Template_Vehicle_Wheel_FNL", 0xd0179265382e61feUL, 0x7e85da89811e2ce7UL),
            ("Template_Vehicle_Wheel_UI_FNL", 0xfa5022e399dc184fUL, 0x7e85da89811e2ce7UL),
            ("Template_tire_ATV_FNL", 0xbdce629c0db47b4cUL, 0x7e85da89811e2ce7UL),
            ("Template_tire_FNL", 0xbdce629c0db47b4cUL, 0x7e85da89811e2ce7UL),
            ("Template_tire_FNL", 0x620de222dd62b8edUL, 0xea1c34ac758fe909UL),
            ("sb_belts", 0xf4a9053dd1b7a1f1UL, 0xa9ec70b9f4146208UL),
        };

        static readonly Dictionary<string, int> byName = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        static readonly Dictionary<ulong, int> byPs = new Dictionary<ulong, int>();
        static readonly Dictionary<(ulong, ulong), int> byPair = new Dictionary<(ulong, ulong), int>();
        static ReflexObjectShading()
        {
            for (int i = 0; i < Programs.Length; i++)
            {
                if (!byName.ContainsKey(Programs[i].name)) byName[Programs[i].name] = i;
                if (!byPs.ContainsKey(Programs[i].ps)) byPs[Programs[i].ps] = i;
                byPair[(Programs[i].ps, Programs[i].vs)] = i;
            }
        }
        public static int ProgramId(string name) => name != null && byName.TryGetValue(name, out int i) ? i : -1;

        // The original renderer's global constants (render-pipeline.md section 3; Shaders/ReflexObjectShading.hlsl): not material values.
        static readonly HashSet<string> Globals = new HashSet<string>(StringComparer.Ordinal)
        {
            "gViewProjection", "gCameraPos", "gDistanceFog", "gDistanceFogColorStart", "gDistanceFogColorEnd", "gDistanceFogColorDelta", "gFogColorDelta",
            "gShadowProjectionMatrix1", "gShadowProjectionMatrix2", "gShadowSeparatingPlane", "gWorldProjectionXZY", "gColorRefProjection",
            "gBackLight", "gBackLightColor", "gSideLightA", "gSideLightAColor", "gSideLightB", "gSideLightBColor", "gFrontLight", "gFrontLightColor",
            "gBounceLightColor", "gSunLight", "gSunLightColor", "gSpecularLightColor", "gColorRefSample", "gWorld", "gBoneMatrixVectors", "g_BoneMatrixVectors"
        };

        /// A shader asset's programs as the material binding needs them.
        public sealed class ProgramInfo
        {
            public string shader; public int id = -1; public ulong psHash, vsHash;
            public readonly Dictionary<string, float[]> constants = new Dictionary<string, float[]>(StringComparer.Ordinal);   // name -> CTAB default (4 floats)
            public readonly Dictionary<int, string> samplers = new Dictionary<int, string>();                                     // register -> CTAB name
            public readonly HashSet<int> cubes = new HashSet<int>();                                                              // dcl_cube registers
        }

        static readonly Dictionary<string, ProgramInfo> infos = new Dictionary<string, ProgramInfo>(StringComparer.OrdinalIgnoreCase);
        static readonly Dictionary<int, HashSet<string>> constantsOf = new Dictionary<int, HashSet<string>>();

        /// Whether a Reflex/Vehicle material's program declares a constant (e.g. usr_mud_amount: the program's mud term).
        public static bool Declares(Material m, string constant)
        {
            if (!m || !m.HasProperty(ProgramProp)) return false;
            lock (infos) return constantsOf.TryGetValue((int)m.GetFloat(ProgramProp), out var c) && c.Contains(constant);
        }
        /// The shader asset name of a Reflex/Vehicle material's program ("?" for another material).
        public static string ProgramName(Material m)
        {
            int id = m && m.HasProperty(ProgramProp) ? (int)m.GetFloat(ProgramProp) : -1;
            return id >= 0 && id < Programs.Length ? Programs[id].name : "?";
        }

        /// The shader asset bytes of a material's PixelShader: its own package, the package's database, then any database of the install
        /// (the object shader assets are spread over the vehicle, gear and level databases; same-named copies are the same programs).
        static byte[] ShaderBytes(Rfx.Install install, Rfx.Package own, string shader)
        {
            var a = own?.Find(shader, "shader");
            if (a == null && own != null) foreach (var q in own.database.packages.Values) { a = q.Find(shader, "shader"); if (a != null) break; }
            if (a == null) a = ShaderIndex(install, shader);
            return a != null ? install.Read(a) : null;
        }

        static Dictionary<string, Rfx.Asset> shaderIndex;
        static Rfx.Asset ShaderIndex(Rfx.Install install, string shader)
        {
            if (shaderIndex == null)
            {
                shaderIndex = new Dictionary<string, Rfx.Asset>(StringComparer.OrdinalIgnoreCase);
                try
                {
                    foreach (string f in RxFiles.GetFiles(install.dir, "*.dx9.database").OrderBy(x => x, StringComparer.Ordinal))
                    {
                        var db = install.Db(System.IO.Path.GetFileName(f).Replace(".dx9.database", ""));
                        if (db == null) continue;
                        foreach (var p in db.packages.Values) foreach (var s in p.OfType("shader")) if (!shaderIndex.ContainsKey(s.name)) shaderIndex[s.name] = s;
                    }
                }
                catch (Exception e) when (e is System.IO.IOException || e is UnauthorizedAccessException) { Debug.LogWarning("Reflex shader index: " + e.Message); }
            }
            return shaderIndex.TryGetValue(shader, out var a) ? a : null;
        }

        public static ulong Fnv(byte[] b, int at, int n)
        {
            ulong h = 1469598103934665603UL;
            for (int i = 0; i < n; i++) { h ^= b[at + i]; h *= 1099511628211UL; }
            return h;
        }

        /// The program pair of a shader asset (cached by name). Null: no such shader asset.
        internal static ProgramInfo Info(Rfx.Install install, Rfx.Package own, string shader)
        {
            if (string.IsNullOrEmpty(shader)) return null;
            lock (infos) if (infos.TryGetValue(shader, out var cached)) return cached;
            var b = ShaderBytes(install, own, shader);
            ProgramInfo info = null;
            if (b != null && b.Length >= 8 && BitConverter.ToInt32(b, 0) == 2)
            {
                info = new ProgramInfo { shader = shader };
                int count = BitConverter.ToInt32(b, 4), p = 8;
                for (int e = 0; e < count && p + 8 <= b.Length; e++)
                {
                    int kind = BitConverter.ToInt32(b, p), len = BitConverter.ToInt32(b, p + 4); p += 8;
                    string entry = len > 1 && p + len <= b.Length ? System.Text.Encoding.ASCII.GetString(b, p, len - 1) : ""; p += len;
                    int size = BitConverter.ToInt32(b, p); p += 4;
                    if (size <= 0 || p + size > b.Length) break;
                    if (kind == 1) { info.psHash = Fnv(b, p, size); Scan(b, p, size, info, true); }
                    else if (entry == "StaticVertexShader") { info.vsHash = Fnv(b, p, size); Scan(b, p, size, info, false); }
                    p += size;
                }
                // the table's program: by the two programs' bytes (same-named copies of a shader asset differ between databases: the
                // level packages' Template_Body_* / Template_Crowd_* are not the material viewer's), else by the pixel program, else the name
                if (byPair.TryGetValue((info.psHash, info.vsHash), out int id)) info.id = id;
                else if (byPs.TryGetValue(info.psHash, out id)) { info.id = id; Debug.LogWarning($"Reflex object shading: {shader}'s vertex program is not the ported one"); }
                else if (byName.TryGetValue(shader, out id)) { info.id = id; Debug.LogWarning($"Reflex object shading: {shader} differs from the ported program of that name"); }
                else Debug.LogWarning($"Reflex object shading: no ported program for {shader}");
            }
            lock (infos)
            {
                infos[shader] = info;
                if (info != null && info.id >= 0 && !constantsOf.ContainsKey(info.id)) constantsOf[info.id] = new HashSet<string>(info.constants.Keys, StringComparer.Ordinal);
            }
            return info;
        }

        // CTAB float constants (defaults) and samplers, and the dcl_cube samplers of one SM3 program.
        static void Scan(byte[] b, int code, int size, ProgramInfo info, bool pixel)
        {
            int end = code + size;
            for (int q = code + 4; q + 4 <= end;)
            {
                uint tok = BitConverter.ToUInt32(b, q);
                int op = (int)(tok & 0xffff);
                if (op == 0xffff) break;
                if (op == 0xfffe)
                {
                    int n = (int)((tok >> 16) & 0x7fff), body = q + 4;
                    if (n >= 7 && b[body] == 'C' && b[body + 1] == 'T' && b[body + 2] == 'A' && b[body + 3] == 'B')
                    {
                        int t = body + 4, cEnd = body + n * 4, consts = BitConverter.ToInt32(b, t + 12), at = BitConverter.ToInt32(b, t + 16);
                        for (int i = 0; i < consts; i++)
                        {
                            int ci = t + at + i * 20; if (ci + 20 > cEnd) break;
                            int nameAt = BitConverter.ToInt32(b, ci), set = BitConverter.ToUInt16(b, ci + 4), reg = BitConverter.ToUInt16(b, ci + 6), regs = BitConverter.ToUInt16(b, ci + 8);
                            int def = BitConverter.ToInt32(b, ci + 16);
                            string name = CString(b, t + nameAt, cEnd);
                            if (set == 3) { if (pixel) info.samplers[reg] = name; continue; }
                            if (set != 2 || Globals.Contains(name) || info.constants.ContainsKey(name)) continue;
                            var v = new float[4];
                            if (def > 0 && t + def + 16 <= cEnd) for (int k = 0; k < 4; k++) v[k] = BitConverter.ToSingle(b, t + def + 4 * k);
                            info.constants[name] = v;
                        }
                    }
                    q = body + n * 4; continue;
                }
                int args = (int)((tok >> 24) & 0xf);
                if (op == 31 && pixel && args == 2 && q + 12 <= end)   // dcl: sampler type in the first argument's bits 27..30 (3 = cube)
                {
                    uint usage = BitConverter.ToUInt32(b, q + 4), reg = BitConverter.ToUInt32(b, q + 8);
                    int regType = (int)(((reg >> 28) & 7) | ((reg >> 8) & 0x18));
                    if (regType == 10 && ((usage >> 27) & 0xf) == 3) info.cubes.Add((int)(reg & 0x7ff));
                }
                q += 4 + args * 4;
            }
        }

        static string CString(byte[] b, int p, int end)
        {
            int e = p; while (e < end && e < b.Length && b[e] != 0) e++;
            return p >= 0 && p < e ? System.Text.Encoding.ASCII.GetString(b, p, e - p) : "";
        }

        static readonly int ProgramProp = Shader.PropertyToID("_Program");
        static readonly string[] GroupKeywords = Enumerable.Range(0, 9).Select(g => "_RFXOBJ_G" + g).ToArray();

        /// _Program and its group's variant keyword (Shaders/ReflexVehicle.shader: one group of 16 programs per variant).
        internal static void SetProgram(Material m, int id)
        {
            m.SetFloat(ProgramProp, id);
            int g = id >= 0 ? id >> 4 : -1;
            for (int i = 0; i < GroupKeywords.Length; i++)
                if (i == g) m.EnableKeyword(GroupKeywords[i]); else m.DisableKeyword(GroupKeywords[i]);
        }
        static readonly int[] TexProps = Enumerable.Range(0, 12).Select(i => Shader.PropertyToID("_Tex" + i)).ToArray();
        static readonly int[] CubeProps = Enumerable.Range(0, 12).Select(i => Shader.PropertyToID("_Cube" + i)).ToArray();

        /// D3DBLEND -> Unity BlendMode.
        static BlendMode Blend(int d3d) => d3d switch
        {
            1 => BlendMode.Zero, 2 => BlendMode.One, 3 => BlendMode.SrcColor, 4 => BlendMode.OneMinusSrcColor, 5 => BlendMode.SrcAlpha, 6 => BlendMode.OneMinusSrcAlpha,
            7 => BlendMode.DstAlpha, 8 => BlendMode.OneMinusDstAlpha, 9 => BlendMode.DstColor, 10 => BlendMode.OneMinusDstColor, 11 => BlendMode.SrcAlphaSaturate, _ => BlendMode.One
        };

        /// The Reflex/Vehicle material of a game material, bound as the game binds it (header). stickers: the bike's Forks texture package
        /// (its texture replaces the frame material's fork sticker, VehicleCatalog.Sticker). Null def: Editor_DiffuseColor in grey.
        internal static Material Build(VehicleCatalog cat, VehicleMaterialDef d, string label, Rfx.Package stickers = null)
        {
            var m = new Material(ReflexVehicleShader.Get()) { name = label ?? d?.name ?? "Reflex object", hideFlags = HideFlags.DontSave };
            var info = d != null ? Info(cat.install, d.package, d.pixelShader) : null;
            if (info == null || info.id < 0)
            {
                if (d != null) Debug.LogWarning($"Reflex object shading: {d.Key} has no ported program ({d.pixelShader})");
                UnityEngine.Object.DestroyImmediate(m);
                return Fallback(new Color(0.5f, 0.5f, 0.5f), label ?? d?.name ?? "Reflex object");
            }
            SetProgram(m, info.id);
            ReflexMaterialClasses.Apply(m, Programs[info.id].name, d, label);   // MATERIALS: ENHANCED: the material's class (nothing happens without it)
            foreach (var kv in info.constants)
            {
                var v = kv.Value; var p = d.Param(kv.Key);
                var c = new Vector4(v[0], v[1], v[2], v[3]);
                if (p != null) for (int k = 0; k < Math.Min(4, p.Length); k++) c[k] = p[k];
                m.SetVector(kv.Key, c);
            }
            for (int i = 0; i < d.textures.Count && i < TexProps.Length; i++)
            {
                if (!info.samplers.ContainsKey(i)) continue;   // the program does not sample it
                var t = d.textures[i]; string tn = (t.name ?? "").ToLowerInvariant();
                // the run-time name / number plates (Numbers, Rider_NamNum_Plate, Rider_CP_NamNum_Plate: textures the game makes at run time,
                // MXPlayer + 0xb00 / 0xb24 / 0xb28; the files hold placeholders): clear until ReflexPlates binds the rider's own
                if (tn.Contains("namnum") || tn == "numbers")
                {
                    m.SetTexture(TexProps[i], VehicleCatalog.ClearTexture);
                    m.SetOverrideTag(ReflexPlates.Tag, "_Tex" + i + ":" + t.name + ":" + t.addrU + ":" + t.addrV + ":" + t.mipFilter);
                    continue;
                }
                if (info.cubes.Contains(i)) { var c = cat.RawCube(t); if (c) m.SetTexture(CubeProps[i], c); continue; }
                if (info.samplers[i].IndexOf("fork_sticker", StringComparison.OrdinalIgnoreCase) >= 0) t = VehicleCatalog.Sticker(t, stickers);
                var x = cat.RawTexture(t); if (x) m.SetTexture(TexProps[i], x);
            }
            // render states (D3DRS_*): CULLMODE (1 none, 2 CW, 3 CCW: the game's front faces are clockwise = Unity's), ZWRITEENABLE,
            // ALPHATESTENABLE / ALPHAREF / ALPHAFUNC, ALPHABLENDENABLE / SRCBLEND / DESTBLEND
            m.SetFloat("_Cull", d.cull == 1 ? (float)CullMode.Off : d.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);
            m.SetFloat("_ZWrite", d.zwrite ? 1 : 0);
            m.SetFloat("_AlphaTest", d.alphaTest ? 1 : 0); m.SetFloat("_AlphaRef", d.alphaRef & 255); m.SetFloat("_AlphaFunc", d.alphaFunc);
            if (d.alphaBlend)
            {
                m.SetFloat("_SrcBlend", (float)Blend(d.srcBlend)); m.SetFloat("_DstBlend", (float)Blend(d.destBlend));
                m.renderQueue = (int)RenderQueue.Transparent; m.SetOverrideTag("RenderType", "Transparent");
            }
            else
            {
                m.SetFloat("_SrcBlend", (float)BlendMode.One); m.SetFloat("_DstBlend", (float)BlendMode.Zero);
                m.renderQueue = d.alphaTest ? (int)RenderQueue.AlphaTest : (int)RenderQueue.Geometry;
            }
            return m;
        }

        /// A level material (the ambient characters and crowds: Template_Crowd_*, Template_Body_*; ReflexLevelContent.AmbientMaterials)
        /// on its ported program, its textures through the level's own loader (textureFor: the level's textures with the material's
        /// sampler states, the crew chief's shirt from the bike's kit ...). Null when its pixel shader has no ported program (the
        /// scenery factory takes it).
        internal static Material BuildLevel(ReflexLevelContent content, string name, ReflexLevelContent.LevelMaterial lm, Func<string, Dictionary<int, float>, Texture2D> textureFor)
        {
            if (content == null || lm == null || string.IsNullOrEmpty(lm.pixelShader)) return null;
            var asset = content.FindAsset(name, "material");
            var info = Info(VehicleCatalog.Shared.install, asset?.package, lm.pixelShader);
            if (info == null || info.id < 0) return null;
            var d = new VehicleMaterialDef { db = asset?.package.database.name ?? "", pkg = asset?.package.name ?? "", name = name, package = asset?.package, pixelShader = lm.pixelShader,
                cull = lm.cull, zwrite = lm.zWrite, alphaTest = lm.alphaTest, alphaRef = lm.alphaRef, alphaFunc = lm.alphaFunc, alphaBlend = lm.alphaBlend, srcBlend = lm.srcBlend, destBlend = lm.dstBlend };
            foreach (var kv in lm.parameters) d.parameters[kv.Key] = kv.Value;
            foreach (string t in lm.textures) d.textures.Add(new VehicleMaterialDef.Tex { name = t });   // (no asset: bound below by the level's loader)
            var m = Build(VehicleCatalog.Shared, d, name + " (object)");
            for (int i = 0; i < lm.textures.Count && i < TexProps.Length; i++)
            {
                if (!info.samplers.ContainsKey(i) || info.cubes.Contains(i) || string.IsNullOrEmpty(lm.textures[i])) continue;
                var t = textureFor(lm.textures[i], i < lm.samplerStates.Count ? lm.samplerStates[i] : null);
                if (t) m.SetTexture(TexProps[i], t);
            }
            return m;
        }

        /// A plain material of one colour (Editor_DiffuseColor: usr_diffuse_color lit by the level), for stand-ins.
        public static Material Fallback(Color c, string name = "Reflex object (plain)")
        {
            var m = new Material(ReflexVehicleShader.Get()) { name = name, hideFlags = HideFlags.DontSave };
            SetProgram(m, ProgramId("Editor_DiffuseColor"));
            m.SetVector("usr_diffuse_color", new Vector4(c.r, c.g, c.b, 1));
            return m;
        }

        static ReflexLevelContent frontEnd;
        /// The front end's garage (MXGarage: the Vehicle Select / MOTOCAREER preview) lit as the game lights it: the UI_Compound scene's
        /// AmbientDirectionalLight, fog and colour reference (ReflexScenery.ApplyShaderGlobals, no volume shadow) and its reflection map.
        public static void ApplyFrontEndGlobals()
        {
            try
            {
                frontEnd ??= ReflexLevelContent.Load("UI_Compound", new ReflexLevelContent.Options { scenery = false, terrain = false, tracks = false });
                frontEnd.ApplyShaderGlobals(false);
                ApplyLevelGlobals(frontEnd);
            }
            catch (Exception e) when (e is System.IO.IOException || e is System.IO.InvalidDataException) { Debug.LogWarning("Reflex object shading: the front end's lighting: " + e.Message); }
        }

        // ------------------------------------------------------------------------------------------- verification

        /// Editor/Tests/ObjShadeTests (the replay of recorded draws): the game material of a draw - a material of its textures' packages
        /// (and their databases' packages) whose pixel shader is the program and whose textures are these (sampler -> asset) - built as the
        /// port builds it. Null when none matches.
        public static Material MaterialFor(string program, IList<(int sampler, string db, string pkg, string name)> textures, IDictionary<string, Vector4> constants, out string key, IEnumerable<string> extraDbs = null)
        {
            key = null; var cat = VehicleCatalog.Shared;
            var pkgs = new List<(string db, string pkg)>();
            foreach (var t in textures) if (!pkgs.Contains((t.db, t.pkg))) pkgs.Add((t.db, t.pkg));
            foreach (string db in extraDbs ?? Enumerable.Empty<string>()) { var d = cat.install.Db(db); if (d != null) foreach (var q in d.packages.Values) if (!pkgs.Contains((db, q.name))) pkgs.Add((db, q.name)); }
            foreach (var (db, pkg) in pkgs.ToList()) { var p = cat.install.Pkg(db, pkg); if (p != null) foreach (var q in p.database.packages.Values) if (!pkgs.Contains((db, q.name))) pkgs.Add((db, q.name)); }
            VehicleMaterialDef best = null; int bestMiss = int.MaxValue;
            foreach (var (db, pkg) in pkgs)
                foreach (string mn in cat.MaterialNames(db, pkg))
                {
                    var def = cat.Material(db, pkg, mn);
                    if (def == null || !string.Equals(def.pixelShader, program, StringComparison.OrdinalIgnoreCase)) continue;
                    if (textures.Any(t => t.sampler >= def.textures.Count || !string.Equals(def.textures[t.sampler].name, t.name, StringComparison.OrdinalIgnoreCase))) continue;
                    // among same-textured materials, the one whose parameters are the draw's (the run-time mud constants aside)
                    int miss = 0;
                    if (constants != null)
                        foreach (var kv in constants)
                        {
                            if (kv.Key.StartsWith("usr_mud_", StringComparison.Ordinal)) continue;
                            var p = def.Param(kv.Key); if (p == null) continue;
                            for (int k = 0; k < Math.Min(4, p.Length); k++) if (Math.Abs(p[k] - kv.Value[k]) > 1e-4f) { miss++; break; }
                        }
                    if (miss < bestMiss) { best = def; bestMiss = miss; }
                }
            if (best == null) return null;
            key = best.Key;
            return cat.Build(best, best.Key);
        }

        /// The material names of a package, in package order (the override rule's iteration order).
        public static string[] MaterialNamesOf(string db, string pkg) => VehicleCatalog.Shared.MaterialNames(db, pkg).ToArray();
        /// The model names of a package.
        public static string[] ModelNamesOf(string db, string pkg) { var p = VehicleCatalog.Shared.install.Pkg(db, pkg); return p == null ? new string[0] : p.OfType("model").Select(a => a.name).ToArray(); }

        /// A texture of the install as the object programs sample it (raw; a cube texture as a Cubemap), with these sampler states.
        public static Texture InstallTexture(string db, string pkg, string name, int addrU = 1, int addrV = 1, int magFilter = 2, int minFilter = 2, int mipFilter = 2, int maxAniso = 1)
        {
            var cat = VehicleCatalog.Shared; var a = cat.install.Find(db, pkg, name, "texture"); if (a == null) return null;
            var t = new VehicleMaterialDef.Tex { name = name, asset = a, addrU = addrU, addrV = addrV, magFilter = magFilter, minFilter = minFilter, mipFilter = mipFilter, maxAniso = maxAniso };
            return (Texture)cat.RawTexture(t) ?? cat.RawCube(t);
        }

        /// Verification: a cube texture of the install with its faces' rows (flip & 1) / columns (flip & 2) reversed (FP16 / 32-bit texels).
        public static Cubemap InstallCube(string db, string pkg, string name, int flip)
        {
            var cat = VehicleCatalog.Shared; var a = cat.install.Find(db, pkg, name, "texture"); if (a == null) return null;
            var t = ReflexTex.Parse(name, cat.install.Read(a)); if (t == null || !t.cube) return null;
            int bpp = t.format == TextureFormat.RGBAHalf ? 8 : t.format == TextureFormat.BGRA32 ? 4 : 0;
            if (bpp > 0 && flip != 0)
                for (int i = 0; i < t.mips.Count; i++)
                {
                    int w = Math.Max(1, t.width >> (i % t.levels)); var src = t.mips[i]; var dst = new byte[src.Length];
                    for (int y = 0; y < w; y++) for (int x = 0; x < w; x++)
                    {
                        int sy = (flip & 1) != 0 ? w - 1 - y : y, sx = (flip & 2) != 0 ? w - 1 - x : x;
                        Buffer.BlockCopy(src, (sy * w + sx) * bpp, dst, (y * w + x) * bpp, bpp);
                    }
                    t.mips[i] = dst;
                }
            return ReflexTex.ToCubemap(t);
        }

        // ------------------------------------------------------------------------------------------- run-time constants

        /// A constant the game sets on an object's materials at run time (gTextureOffset of the belts, usr_fade_value of the flywheels,
        /// usr_damage*): set on every material of the renderers that declares it.
        public static void SetRuntime(IEnumerable<Renderer> renderers, string constant, Vector4 value)
        {
            if (renderers == null) return;
            int id = Shader.PropertyToID(constant);
            foreach (var r in renderers) { if (!r) continue; foreach (var m in r.sharedMaterials) if (m && m.HasProperty(id)) m.SetVector(id, value); }
        }

        /// The vehicle's run-time constants (ReflexVehicles.VehicleConstants: the game's override list V+0x6c0, collected with the node
        /// flags on the simulation thread) on its model's materials: the Super Buggy's belt scroll gTextureOffset (V+0x15a0) and flywheel
        /// blur usr_fade_value (V+0x15b0), any other entry the game lists (the mud entries are ReflexMud's). Main thread, every frame.
        public static void ApplyVehicleConstants(uint vehicle, ReflexBikeModel bike) => ApplyVehicleConstants(vehicle, bike, null);
        /// The same with the bike's renderers collected once by the caller (no search and no garbage a frame; RIDER CAP, OriginalOpponents).
        public static void ApplyVehicleConstants(uint vehicle, ReflexBikeModel bike, Renderer[] renderers)
        {
            if (bike == null || vehicle == 0 || !ReflexVehicles.TryConstants(vehicle, out var names, out var values) || names.Length == 0) return;
            if (renderers != null)
            {
                foreach (var r in renderers)
                    if (r) foreach (var m in r.sharedMaterials)
                        if (m) for (int i = 0; i < names.Length; i++) if (Declares(m, names[i])) m.SetVector(names[i], values[i]);
                return;
            }
            foreach (var root in new[] { bike.chassis ? bike.chassis.transform : null, bike.frontWheel, bike.rearWheel })
            {
                if (!root) continue;
                foreach (var r in root.GetComponentsInChildren<Renderer>(true))
                    foreach (var m in r.sharedMaterials)
                        if (m) for (int i = 0; i < names.Length; i++) if (Declares(m, names[i])) m.SetVector(names[i], values[i]);
            }
        }

        // ------------------------------------------------------------------------------------------- level globals

        static readonly int VsmOn = Shader.PropertyToID("_RfxVsmOn"), ReflectionOn = Shader.PropertyToID("_RfxReflectionOn"), ReflectionCube = Shader.PropertyToID("_RfxReflectionCube"),
            ColorRefOn = Shader.PropertyToID("_RfxColorRefOn"), ColorRefMap = Shader.PropertyToID("_RfxColorRefMap");
        static Cubemap levelReflection; static Texture2D levelColorRef;

        /// The object programs' level inputs beyond ReflexScenery.ApplyShaderGlobals (OriginalLevel calls it after them): the level's
        /// reflection map (s14 samplerReflectionMap; none: black, as an unbound sampler), its colour reference map (s13
        /// samplerAmbientDirectionalLightColorRef, AmbientDirectionalLight _ColorReferenceMap, sampled at gColorRefProjection: a real
        /// map on Kingston, Editor_Default_White elsewhere) and the dynamic shadow source (Unity's main-light shadow through the
        /// programs' own Chebyshev term).
        internal static string ApplyLevelGlobals(ReflexLevelContent content)
        {
            if (levelReflection) UnityEngine.Object.Destroy(levelReflection);
            if (levelColorRef) UnityEngine.Object.Destroy(levelColorRef);
            levelReflection = null; levelColorRef = null;
            string refl = null, cref = content?.ColorReferenceName;
            if (content != null)
            {
                // the reflection map: an entity's asset field naming a *ReflectionMap texture, else the level's texture of that name
                foreach (var e in content.entities ?? new List<SceneEntity>())
                {
                    foreach (var f in e.rsdata.children)
                        if (f.children.Count > 0 && f.children[0].S("name") is string n && n.EndsWith("ReflectionMap", StringComparison.OrdinalIgnoreCase)) { refl = n; break; }
                    if (refl != null) break;
                }
                if (refl == null) foreach (var p in content.LevelPackages) { var a = p.OfType("texture").FirstOrDefault(t => t.name.EndsWith("ReflectionMap", StringComparison.OrdinalIgnoreCase)); if (a != null) { refl = a.name; break; } }
                try
                {
                    if (refl != null) { var t = ReflexTex.Parse(refl, content.ReadAsset(refl, "texture")); levelReflection = t != null && t.cube ? ReflexTex.ToCubemap(t) : null; }
                    if (content.RestoredColorRef != null)   // RESTORATION / PORT ADDITION (w-fresnolook): the RESTORED LOOK donor's map (RestoredLook.cs)
                    { cref = content.RestoredColorRef.name; levelColorRef = ReflexTex.ToTexture(content.RestoredColorRef, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Trilinear); }
                    else if (cref != null) { var t = ReflexTex.Parse(cref, content.ReadAsset(cref, "texture")); levelColorRef = t != null && !t.cube ? ReflexTex.ToTexture(t, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Trilinear) : null; }
                }
                catch (Exception e) when (e is System.IO.IOException || e is System.IO.InvalidDataException) { Debug.LogWarning("Reflex object shading: " + e.Message); }
            }
            Shader.SetGlobalFloat(VsmOn, 0);
            Shader.SetGlobalFloat(ReflectionOn, levelReflection ? 1 : 0);
            if (levelReflection) Shader.SetGlobalTexture(ReflectionCube, levelReflection);
            Shader.SetGlobalFloat(ColorRefOn, levelColorRef ? 1 : 0);
            if (levelColorRef) Shader.SetGlobalTexture(ColorRefMap, levelColorRef);
            return $"reflection {(levelReflection ? refl : "none")}, colour reference {(levelColorRef ? cref : "constant")}";
        }
    }
}

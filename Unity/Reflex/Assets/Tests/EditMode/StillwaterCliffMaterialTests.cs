using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class StillwaterCliffMaterialTests
    {
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        [TestCase("Template_DF_X2_VXB_AO_NM")]
        public void OriginalVertexBlendCliffKeepsDistinctDiffuseRolesAndOneSidedNormal(string program)
        {
            var content = Find("Reflex.Port.Content.ReflexLevelContent");
            var lmType = content.GetNestedType("LevelMaterial", BindingFlags.NonPublic);
            var lm = Activator.CreateInstance(lmType, true);
            lmType.GetField("name").SetValue(lm, program);
            lmType.GetField("pixelShader").SetValue(lm, program);
            var names = (List<string>)lmType.GetField("textures").GetValue(lm);
            var states = (List<Dictionary<int,float>>)lmType.GetField("samplerStates").GetValue(lm);
            lmType.GetField("userSamplers").SetValue(lm, new[]{"usr_sampler_ambient_occlusion_map","usr_sampler_diffuse_map_2","usr_sampler_diffuse_texture","usr_sampler_normal_map"});
            var textures = new Dictionary<string,Texture2D>(); Material material = null;
            try
            {
                foreach (var name in new[]{"ao","blend-target","base-diffuse","normal"})
                { names.Add(name); states.Add(new Dictionary<int,float>()); textures[name] = new Texture2D(1,1); }
                var shadersType = content.GetNestedType("SceneryShaders");
                var shaders = Activator.CreateInstance(shadersType);
                var shader = Shader.Find("Reflex/Original Scenery"); Assert.IsNotNull(shader);
                shadersType.GetField("scenery").SetValue(shaders, shader);
                Func<string,Dictionary<int,float>,Texture2D> load = (name, state) => textures[name];
                material = (Material)Find("Reflex.Port.Content.ReflexSceneryMaterial").GetMethod("Create", BindingFlags.Public|BindingFlags.Static)
                    .Invoke(null, new object[]{null,lm,shaders,load,null});
                Assert.AreEqual(16, material.GetFloat("_Mode"));
                Assert.AreSame(textures["ao"], material.GetTexture("_AOMap"));
                Assert.AreSame(textures["base-diffuse"], material.GetTexture("_BaseMap"));
                Assert.AreSame(textures["blend-target"], material.GetTexture("_DetailMap"));
                Assert.AreSame(textures["normal"], material.GetTexture("_NormalMap"));
                Assert.AreEqual(1,material.GetFloat("_AmbientSrc"));
                Assert.AreEqual(1,material.GetFloat("_UseDynShadow"));
                Assert.AreEqual(1,material.GetFloat("_NormalMode"));
                Assert.AreEqual(1,material.GetFloat("_AOMode"));
                Assert.AreEqual(0,material.GetFloat("_TwoSided"));
                Assert.AreEqual(1,material.GetFloat("_UseVolShadow"));
            }
            finally { if(material) UnityEngine.Object.DestroyImmediate(material); foreach(var t in textures.Values) UnityEngine.Object.DestroyImmediate(t); }
        }
    }
}

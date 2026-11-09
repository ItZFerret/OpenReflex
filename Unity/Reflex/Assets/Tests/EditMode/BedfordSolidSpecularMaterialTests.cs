using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.RxCore.Tests
{
    public class BedfordSolidSpecularMaterialTests
    {
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        static Material Create(Dictionary<string,float[]> parameters)
        {
            var content=Find("Reflex.Port.Content.ReflexLevelContent");
            var lmType=content.GetNestedType("LevelMaterial",BindingFlags.NonPublic);
            var lm=Activator.CreateInstance(lmType,true);
            lmType.GetField("name").SetValue(lm,"TearDrop_Metal");
            lmType.GetField("pixelShader").SetValue(lm,"Template_SC_SP");
            var actual=(Dictionary<string,float[]>)lmType.GetField("parameters").GetValue(lm);
            foreach(var parameter in parameters)actual.Add(parameter.Key,parameter.Value);
            var shadersType=content.GetNestedType("SceneryShaders");
            var shaders=Activator.CreateInstance(shadersType);
            var shader=Shader.Find("Reflex/Original Scenery");Assert.IsNotNull(shader);
            shadersType.GetField("scenery").SetValue(shaders,shader);
            Func<string,Dictionary<int,float>,Texture2D> noTexture=(name,state)=>throw new AssertionException("SC_SP has no user texture sampler: "+name);
            return (Material)Find("Reflex.Port.Content.ReflexSceneryMaterial").GetMethod("Create",BindingFlags.Public|BindingFlags.Static)
                .Invoke(null,new object[]{null,lm,shaders,noTexture,null});
        }
        static void CheckProgram(Material material)
        {
            Assert.AreEqual(0,material.GetFloat("_Mode"));
            Assert.AreEqual(2,material.GetFloat("_DiffuseSrc"));
            Assert.AreEqual(0,material.GetFloat("_AmbientSrc")); // original vertex ambient, not a per-pixel substitute
            Assert.AreEqual(0,material.GetFloat("_NormalMode"));
            Assert.AreEqual(0,material.GetFloat("_AOMode"));
            Assert.AreEqual(0,material.GetFloat("_TwoSided"));
            Assert.AreEqual(1,material.GetFloat("_UseDynShadow"));
            Assert.AreEqual(1,material.GetFloat("_UseVolShadow"));
            Assert.AreEqual(0,material.GetVector("_EnvParams").y);
            Assert.AreEqual(0,material.GetFloat("_AlphaSource"));
            Assert.AreEqual(0,material.GetFloat("_AlphaTest"));
            Assert.AreEqual((float)CullMode.Back,material.GetFloat("_Cull"));
            Assert.AreEqual(1,material.GetFloat("_ZWrite"));
            Assert.AreEqual((float)BlendMode.One,material.GetFloat("_SrcBlend"));
            Assert.AreEqual((float)BlendMode.Zero,material.GetFloat("_DstBlend"));
        }
        [Test]
        public void BedfordTearDropMetalBindsAuthoredDarkColourAndSunColouredSpecular()
        {
            Material material=null;
            try
            {
                material=Create(new Dictionary<string,float[]> {
                    ["usr_solid_color"]=new[]{0.1206896529f,0.1167133003f,0.1141002774f,1f},
                    ["usr_specular_hardness"]=new[]{7.441860676f,7.441860676f,7.441860676f,7.441860676f},
                    ["usr_specular_color"]=new[]{1f,1f,1f,1f}
                });
                CheckProgram(material);
                Assert.AreEqual(new Vector4(0.1206896529f,0.1167133003f,0.1141002774f,1f),material.GetVector("_BaseColor"));
                Assert.AreEqual(new Vector4(7.441860676f,1f,1f,1f),material.GetVector("_SpecParams"));
                Assert.AreEqual(Vector4.one,material.GetVector("_SpecTint"));
            }
            finally {if(material) UnityEngine.Object.DestroyImmediate(material);}
        }
        [Test]
        public void MissingAuthoredParametersUseActualScSpConstantTableDefaults()
        {
            Material material=null;
            try
            {
                material=Create(new Dictionary<string,float[]>());CheckProgram(material);
                Assert.AreEqual(new Vector4(0.349020004f,0.847059011f,0.294117987f,1f),material.GetVector("_BaseColor"));
                Assert.AreEqual(new Vector4(5f,1f,1f,1f),material.GetVector("_SpecParams"));
                Assert.AreEqual(Vector4.one,material.GetVector("_SpecTint"));
            }
            finally {if(material) UnityEngine.Object.DestroyImmediate(material);}
        }
    }
}

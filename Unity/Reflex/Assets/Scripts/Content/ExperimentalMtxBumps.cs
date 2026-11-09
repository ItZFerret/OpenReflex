using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using Reflex.Port.Rendering;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        Material mtxTerrainMaterial;

        // The same three layers as a native Reflex venue: surface bump, authored rut
        // overlays, then deformation reveal. The compressed mod supplies the course.
        List<ReflexRTBump.Draw> BuildMtxBumpModels(Shader shader)
        {
            var draws=new List<ReflexRTBump.Draw>();
            Rfx.Check(mtxTerrainMaterial,"MTX terrain must be built before its bump overlays");
            float scale=Reflex.Port.RxCore.RxMapScale.Current,span=4094f*scale,half=span*.5f;
            bool paradise=TrackMods.Bound?.id!="McKennaRock";
            Texture2D Tex(string name)
            {
                var t=TerrainTexture(name,terrainData.material.samplerStates[1],DetailAniso);
                Rfx.Check(t,"Missing MTX bump texture "+name);return t;
            }
            Material Mat(string pass)
            {
                var m=new Material(shader){name="MTX "+pass,hideFlags=HideFlags.DontSave};
                m.SetFloat("_Cull",(float)CullMode.Off);Track(m);return m;
            }
            var quad=new Mesh{name="MTX terrain bump coverage",hideFlags=HideFlags.DontSave};
            quad.vertices=new[]{new Vector3(-half,0,-half),new Vector3(half,0,-half),new Vector3(-half,0,half),new Vector3(half,0,half)};
            quad.uv=new[]{new Vector2(0,0),new Vector2(1,0),new Vector2(0,1),new Vector2(1,1)};
            quad.colors=new[]{Color.white,Color.white,Color.white,Color.white};
            quad.triangles=new[]{0,2,1,1,2,3};quad.RecalculateBounds();Track(quad);
            void Add(Mesh mesh,Material material,string pass,string name)
            {
                int index=material.FindPass(pass);Rfx.Check(index>=0,"MTX bump shader pass "+pass);
                draws.Add(new ReflexRTBump.Draw{mesh=mesh,material=material,pass=index,matrix=Matrix4x4.identity,name=name});
            }
            var basic=Mat("HFB_4tex");
            basic.SetTexture("_Bump0",Tex(paradise?"NAT_BE_Dirt01_BumpNew":"NAT_CO_B_SandBUMP"));
            basic.SetTexture("_Bump1",Tex(paradise?"NAT_BE_Dirt02_Bump":"NAT_CO_B_DirtLightBUMP"));
            basic.SetTexture("_Bump2",Tex("NAT_CO_B_SandInsideBUMP"));
            basic.SetTexture("_Bump3",Tex("NAT_CO_B_GrassBUMPS"));
            basic.SetTexture("_Mask",mtxTerrainMaterial.GetTexture("_DetailMask"));
            basic.SetVector("_TileRate",mtxTerrainMaterial.GetVector("_DetailTile"));
            Add(quad,basic,"HFB_4tex","MTX surface relief");

            var course=ExperimentalTerrain.Info(TrackMods.Bound.folder).course;
            Rfx.Check(course!=null && course.Length>=20 && course.Length%5==0,"MTX rut course");
            int count=course.Length/5;
            Vector3 Point(int i) { i=(i%count+count)%count;return new Vector3(course[i*5],0,course[i*5+2])*scale; }
            var centers=new List<Vector3>();var widths=new List<float>();
            for(int i=0;i<count;i++)
            {
                var a=Point(i-1);var b=Point(i);var c=Point(i+1);var d=Point(i+2);
                int steps=Mathf.Max(2,Mathf.CeilToInt(Vector3.Distance(b,c)/8f));
                for(int k=0;k<steps;k++)
                {
                    float t=k/(float)steps,t2=t*t,t3=t2*t;
                    centers.Add(.5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t2+(-a+3*b-3*c+d)*t3));
                    widths.Add(Mathf.Clamp(Mathf.Lerp(course[i*5+3],course[((i+1)%count)*5+3],t)*.4f,12,42)*scale);
                }
            }
            int rows=centers.Count;var distance=new float[rows+1];
            for(int i=1;i<=rows;i++)distance[i]=distance[i-1]+Vector3.Distance(centers[i%rows],centers[i-1]);
            float repeats=Mathf.Max(1,Mathf.Round(distance[rows]/48f));
            const int columns=9;
            var vertices=new Vector3[(rows+1)*columns];var uv=new Vector2[vertices.Length];var colors=new Color[vertices.Length];
            var triangles=new int[rows*(columns-1)*6];int at=0;
            for(int i=0;i<=rows;i++)
            {
                int r=i%rows;
                var forward=(centers[(r+1)%rows]-centers[(r+rows-1)%rows]).normalized;
                var side=new Vector3(forward.z,0,-forward.x);
                for(int j=0;j<columns;j++)
                {
                    float u=j/(float)(columns-1),offset=(2*u-1)*widths[r];int v=i*columns+j;
                    vertices[v]=centers[r]+side*offset;
                    // Keep groove spacing in world feet as the track is resized. The V seam repeats exactly.
                    uv[v]=new Vector2(offset/24f+.5f,1-distance[i]/distance[rows]*repeats);
                    colors[v]=new Color(1,1,1,.75f*Mathf.SmoothStep(0,1,Mathf.Min(u,1-u)*5));
                    if(i<rows && j<columns-1)
                    {
                        triangles[at++]=v;triangles[at++]=v+columns;triangles[at++]=v+1;
                        triangles[at++]=v+1;triangles[at++]=v+columns;triangles[at++]=v+columns+1;
                    }
                }
            }
            var ribbon=new Mesh{name="MTX course-following rut ribbons",hideFlags=HideFlags.DontSave,indexFormat=IndexFormat.UInt32};
            ribbon.vertices=vertices;ribbon.uv=uv;ribbon.colors=colors;ribbon.triangles=triangles;ribbon.RecalculateBounds();Track(ribbon);
            var ruts=Mat("MTX_Ruts");ruts.SetTexture("_Bump0",Tex("NAT_CO_B_Ruts"));
            ruts.SetTexture("_Mask",mtxTerrainMaterial.GetTexture("_DetailMask"));ruts.SetFloat("_TerrainSize",span);
            // McKenna's legacy mask uses green for off-course; Paradise's blue/green are grass/rock.
            ruts.SetFloat("_Tuner",paradise?1:0);
            Add(ribbon,ruts,"MTX_Ruts","MTX course ruts");
            var reveal=Mat("HFB_1tex_deformed");reveal.SetTexture("_Bump0",Tex("HFT_Default_Deform_Reveal"));
            reveal.SetVector("_TileRate",Vector4.one*(160*2*scale));
            Add(quad,reveal,"HFB_1tex_deformed","MTX live deformation reveal");
            Debug.Log($"[MTX] Terrain depth: {draws.Count} native bump layers, {rows} course sections, {triangles.Length/3} rut triangles");
            return draws;
        }
    }
}

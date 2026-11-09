using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using UnityEngine;
namespace Reflex.Port.Content
{
    public static class ExperimentalTerrain
    {
        [Serializable] public sealed class Metadata { public string format, name, kind, id; public int version, raceVersion; public float[] course; }
        public static Metadata Info(string path)
        {
            using var stream=new GZipStream(File.OpenRead(path),CompressionMode.Decompress);
            using var r=new BinaryReader(stream);
            if(r.ReadUInt32()!=0x4b505852 || !PackageVersion(r.ReadInt32())) throw new InvalidDataException("Unsupported RXPK");
            int n=r.ReadInt32(); if(n<2||n>65536) throw new InvalidDataException("Invalid metadata size");
            var m=JsonUtility.FromJson<Metadata>(Encoding.UTF8.GetString(r.ReadBytes(n)));
            if(m==null||m.format!="reflex-mod-track"||m.version!=1) throw new InvalidDataException("Invalid metadata");
            return m;
        }
        public static Texture2D CourseMask(string path)
        {
            const int n=2048; var pixels=new Color32[n*n];
            for(int i=0;i<pixels.Length;i++) pixels[i]=new Color32(0,255,0,0);
            var c=Info(path).course;
            if(c==null||c.Length%5!=0||c.Length>20000) throw new InvalidDataException("Invalid course");
            for(int j=0;j<c.Length;j+=5)
            {
                int k=(j+5)%c.Length;
                var a=new Vector2((c[j]+2047)/2,(2047-c[j+2])/2);
                var b=new Vector2((c[k]+2047)/2,(2047-c[k+2])/2);
                float radius=Mathf.Clamp(Mathf.Max(c[j+3],c[k+3])/4,4,40); var d=b-a;
                int x0=Mathf.Max(0,Mathf.FloorToInt(Mathf.Min(a.x,b.x)-radius)), x1=Mathf.Min(n-1,Mathf.CeilToInt(Mathf.Max(a.x,b.x)+radius));
                int y0=Mathf.Max(0,Mathf.FloorToInt(Mathf.Min(a.y,b.y)-radius)), y1=Mathf.Min(n-1,Mathf.CeilToInt(Mathf.Max(a.y,b.y)+radius));
                for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++)
                {
                    var q=new Vector2(x,y); float t=d.sqrMagnitude>0?Mathf.Clamp01(Vector2.Dot(q-a,d)/d.sqrMagnitude):0;
                    float dist=(q-a-t*d).magnitude;
                    byte green=(byte)(255*Mathf.Clamp01(dist-radius+1));
                    if(green<pixels[y*n+x].g) pixels[y*n+x].g=green;
                }
            }
            var texture=new Texture2D(n,n,TextureFormat.RGBA32,true,true){name="MTX course material mask",wrapMode=TextureWrapMode.Clamp};
            texture.SetPixels32(pixels);texture.Apply(true,true);return texture;
        }
        public static bool For(string level) => Active && string.Equals(level, TrackMods.Bound.level, StringComparison.OrdinalIgnoreCase);
        public static bool Active => TrackMods.Bound?.experimentalTerrain == true;
        static bool PackageVersion(int v) => v>=1 && v<=4;
        static byte[] ReadScenerySection(BinaryReader r)
        {
            if(r.ReadUInt32()!=0x454e4353) throw new InvalidDataException("Missing scenery section");
            int n=r.ReadInt32(); if(n<8 || n>128*1024*1024) throw new InvalidDataException("Invalid scenery length");
            var bytes=r.ReadBytes(n);if(bytes.Length!=n) throw new EndOfStreamException("Truncated scenery");return bytes;
        }
        static void ReadCollisionSection(BinaryReader r)
        {
            if(r.ReadUInt32()!=0x4c4c4f43)throw new InvalidDataException("Missing collision section");
            int n=r.ReadInt32();if(n<12||n>32*1024*1024)throw new InvalidDataException("Invalid collision length");
            while(n>0){int count=Math.Min(n,8192);if(r.ReadBytes(count).Length!=count)throw new EndOfStreamException();n-=count;}
        }
        public static byte[] Scenery(string path)
        {
            using var stream=new GZipStream(File.OpenRead(path),CompressionMode.Decompress);
            using var r=new BinaryReader(stream);
            if(r.ReadUInt32()!=0x4b505852) throw new InvalidDataException("Invalid RXPK");
            int version=r.ReadInt32(); if(!PackageVersion(version)) throw new InvalidDataException("Unsupported RXPK version");
            if(version==1) return null;
            int n=r.ReadInt32(); if(n<2||n>65536) throw new InvalidDataException("Invalid metadata length");
            long skip=n+40L+2048L*2048*4;
            while(skip>0) { int count=(int)Math.Min(skip,8192);if(r.ReadBytes(count).Length!=count)throw new EndOfStreamException();skip-=count; }
            var data=ReadScenerySection(r);ReadCollisionSection(r);if(version>=3)ReadSurface(r);if(version>=4)ReadColour(r);ReadRaceEnd(r);return data;
        }
        static byte[] ReadSurface(BinaryReader r)
        {
            if(r.ReadUInt32()!=0x46525553 || r.ReadUInt32()!=12+2048*2048*4 || r.ReadUInt32()!=0x4d535852 || r.ReadUInt32()!=1 || r.ReadUInt32()!=2048)
                throw new InvalidDataException("Invalid surface map");
            var bytes=r.ReadBytes(2048*2048*4);if(bytes.Length!=2048*2048*4)throw new EndOfStreamException();
            for(int i=0;i<bytes.Length;i+=4)if(bytes[i]+bytes[i+1]+bytes[i+2]>255)throw new InvalidDataException("Invalid surface weights");return bytes;
        }
        public static byte[] Surface(string path) => Maps(path,false);
        public static byte[] Colour(string path) => Maps(path,true);
        static byte[] Maps(string path,bool colour)
        {
            using var stream=new GZipStream(File.OpenRead(path),CompressionMode.Decompress);
            using var r=new BinaryReader(stream);
            if(r.ReadUInt32()!=0x4b505852)throw new InvalidDataException("Invalid RXPK");
            int v=r.ReadInt32();if(!PackageVersion(v))throw new InvalidDataException("Unsupported RXPK");
            if(v<3 || (colour && v<4))return null;
            int n=r.ReadInt32();if(n<2||n>65536)throw new InvalidDataException("Invalid metadata");
            long skip=n+40L+2048L*2048*4;
            while(skip>0){int count=(int)Math.Min(skip,8192);if(r.ReadBytes(count).Length!=count)throw new EndOfStreamException();skip-=count;}
            ReadScenerySection(r);ReadCollisionSection(r);var data=ReadSurface(r);
            if(v>=4){var rgb=ReadColour(r);if(colour)data=rgb;}
            ReadRaceEnd(r);return data;
        }
        static bool ReadRaceEnd(BinaryReader r)
        {
            int first=r.BaseStream.ReadByte();if(first<0)return false;
            if(first!='R'||r.ReadByte()!='A'||r.ReadByte()!='C'||r.ReadByte()!='E')throw new InvalidDataException("Invalid race section");
            uint size=r.ReadUInt32();
            if(size<100||size>164000||r.ReadUInt32()!=0x41525852||r.ReadUInt32()!=1||r.ReadUInt32()>1)throw new InvalidDataException("Invalid RXRA");
            uint lap=r.ReadUInt32(),lead=r.ReadUInt32();
            if(lap<10||lap>4096||lead<2||lead>4096||size!=100+20*(lap+lead))throw new InvalidDataException("Invalid race route");
            for(uint i=0;i<20+5*(lap+lead);i++) { float f=r.ReadSingle();if(!float.IsFinite(f)||Mathf.Abs(f)>100000)throw new InvalidDataException("Invalid race point"); }
            if(r.BaseStream.ReadByte()!=-1)throw new InvalidDataException("Unexpected package data");return true;
        }
        static byte[] ReadColour(BinaryReader r)
        {
            if(r.ReadUInt32()!=0x524c4f43 || r.ReadUInt32()!=12+2048*2048*4 || r.ReadUInt32()!=0x4d435852 || r.ReadUInt32()!=1 || r.ReadUInt32()!=2048)
                throw new InvalidDataException("Invalid terrain colour map");
            var data=r.ReadBytes(2048*2048*4);if(data.Length!=2048*2048*4)throw new EndOfStreamException();return data;
        }
        public static float[] Read(string folder)
        {
            using var compressed = new GZipStream(File.OpenRead(folder),CompressionMode.Decompress);
            using var r = new BinaryReader(compressed);
            if(r.ReadUInt32()!=0x4b505852) throw new InvalidDataException("Unsupported RXPK package");
            int version=r.ReadInt32(); if(!PackageVersion(version)) throw new InvalidDataException("Unsupported RXPK version");
            int count=r.ReadInt32(); if(count<2||count>65536) throw new InvalidDataException("Invalid metadata");
            var metadataBytes=r.ReadBytes(count);if(metadataBytes.Length!=count)throw new EndOfStreamException();
            var metadata=JsonUtility.FromJson<Metadata>(Encoding.UTF8.GetString(metadataBytes));
            if(metadata==null || metadata.format!="reflex-mod-track" || metadata.version!=1 || metadata.raceVersion<0 || metadata.raceVersion>1)throw new InvalidDataException("Invalid metadata");
            if (r.ReadUInt32()!=0x314d5852 || r.ReadInt32()!=1 || r.ReadInt32()!=2048 || r.ReadSingle()!=2)
                throw new InvalidDataException("Unsupported RXM1 terrain");
            for(int i=0;i<6;i++) if(!float.IsFinite(r.ReadSingle())) throw new InvalidDataException("Invalid spawn");
            var h=new float[2048*2048];
            for(int i=0;i<h.Length;i++) { h[i]=r.ReadSingle()/2048f; if(!float.IsFinite(h[i])) throw new InvalidDataException("Invalid height"); }
            if(version>=2) { ReadScenerySection(r); ReadCollisionSection(r); }
            if(version>=3) ReadSurface(r);
            if(version>=4) ReadColour(r);
            if(ReadRaceEnd(r)!=(metadata.raceVersion==1))throw new InvalidDataException("Race metadata does not match package");
            return h;
        }
    }
}

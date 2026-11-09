// BXML (0x103ea zlib / 0x3ea raw) and scene-container readers, shared with the Open Unleashed port (MX vs ATV RE). Content always comes from the player's own Reflex install.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using UnityEngine;

namespace Reflex.Port.Content
{
    internal static class BxmlData
    {
        internal static void Check(bool ok, string detail) { if (!ok) throw new InvalidDataException("Unsupported or damaged Reflex data: " + detail); }
        internal static void Range(byte[] b, int p, long n) => Check(p >= 0 && n >= 0 && p <= b.Length - n, "file range");
        internal static int BE(byte[] b, int p) { Range(b,p,4); return unchecked((int)((uint)b[p]<<24 | (uint)b[p+1]<<16 | (uint)b[p+2]<<8 | b[p+3])); }
        internal static int LE(byte[] b, int p) { Range(b,p,4); return BitConverter.ToInt32(b,p); }
        internal static int U16(byte[] b,int p) { Range(b,p,2); return b[p]<<8 | b[p+1]; }
        internal static float F(byte[] b,int p) => BitConverter.Int32BitsToSingle(BE(b,p));
        internal static Vector3 V(byte[] b,int p) => new Vector3(F(b,p),F(b,p+4),F(b,p+8));
        internal static float Half(byte[] b,int p)
        {
            int v=U16(b,p), e=(v>>10)&31, m=v&1023;
            float f=e==0 ? m/16777216f : e==31 ? float.NaN : (1+m/1024f)*(float)Math.Pow(2,e-15);
            return (v&32768)!=0 ? -f:f;
        }
        internal static string SafeName(string n)
        {
            Check(!string.IsNullOrWhiteSpace(n) && n.Length<=160 && n.All(c=>char.IsLetterOrDigit(c) || "_-. '()&[]".Contains(c)) && n!="." && n!="..", "asset name: "+n);
            return n.ToLowerInvariant();
        }

        internal static byte[] Lzx(byte[] data, CancellationToken cancel)
        {
            int limit=LE(data,0); Check(limit==data.Length-4 && limit>=4,"LZX container");
            using var output=new MemoryStream();
            int p=4;
            while(p<limit)
            {
                cancel.ThrowIfCancellationRequested();
                int size=LE(data,p), packed=LE(data,p+4); p+=8;
                if(packed==0 && p==limit)break; // Empty final chunk on exact 64 KiB boundaries.
                Check(size>0 && size<=65536 && packed>0 && packed<=65600 && p<=limit-packed,"LZX chunk");
                int end=p+packed;
                if(end==limit) { int tail=LE(data,limit); if(tail!=0) size=tail; }
                Check(size>0 && size<=65536 && output.Length+size<=64*1024*1024,"LZX output");
                if(packed==size) { output.Write(data,p,size); p=end; continue; }
                var decoder=new LzxDecoder(17); long before=output.Length;
                while(p<end)
                {
                    int frameSize=32768, frameBytes;
                    if(data[p]==255) { frameSize=U16(data,p+1); frameBytes=U16(data,p+3); p+=5; }
                    else { frameBytes=U16(data,p); p+=2; }
                    Check(frameSize>0 && frameSize<=32768 && frameBytes>0 && p<=end-frameBytes && output.Length-before+frameSize<=size,"LZX frame");
                    using var input=new MemoryStream(data,p,frameBytes,false);
                    Check(decoder.Decompress(input,frameBytes,output,frameSize)==0,"LZX decoding");
                    p+=frameBytes;
                }
                Check(output.Length-before==size,"LZX length");
            }
            return output.ToArray();
        }

        internal sealed class Node
        {
            public string tag; public object value;
            public Dictionary<string,object> attributes=new Dictionary<string,object>();
            public List<Node> children=new List<Node>();
            public string S(string key) => attributes.TryGetValue(key,out var v) ? v as string : null;
            public int I(string key) => attributes.TryGetValue(key,out var v) ? Convert.ToInt32(v) : 0;
            public Node Child(string tag) => children.Find(n=>n.tag==tag);
            public Node Property(string name) => children.Find(n=>n.S("name")==name);
            public Node Leaf => children.Count==1 ? children[0].Leaf : this;
            public string Asset => Leaf.S("name");
            public Matrix4x4 Matrix
            {
                get { var a=Leaf.value as float[]; Check(a?.Length==16,"entity transform"); var m=new Matrix4x4(); for(int i=0;i<16;i++) m[i]=a[i]; return m; }
            }
            public IEnumerable<Node> All() { yield return this; foreach(var c in children) foreach(var n in c.All()) yield return n; }
        }

        internal static Node Xml(byte[] source)
        {
            Check(LE(source,0)==0x4c4d5842,"BXML magic"); int version=LE(source,4), count=LE(source,8), pool=LE(source,12), poolSize=LE(source,16), ac=LE(source,20), nc=LE(source,24);
            // (w-fb-customtrack) the decoded size bounds a BXML, as in core/src/fmt_bxml.c: Mods/Tracks "2014 Monster Energy Cup"'s Base subscene
            // holds 259601 attributes / 157749 nodes (the old 200000 cap refused it; the game reads it)
            Check(count>0 && count<=4000000 && pool>=0 && poolSize>=0 && ac>=0 && ac<=4000000 && nc>0 && nc<=4000000,"BXML counts");
            long expected=(long)pool+poolSize+ac*12L+nc*32L; Check(expected<=32*1024*1024,"BXML size");
            byte[] b=new byte[(int)expected];
            if(version==0x103ea)
            {
                int packed=LE(source,32); Check(packed>=6 && packed==source.Length-36,"BXML compressed length");
                using var stream=new MemoryStream(source,38,packed-6);
                using var zip=new DeflateStream(stream,CompressionMode.Decompress);
                int got=0; while(got<b.Length) { int n=zip.Read(b,got,b.Length-got); Check(n>0,"BXML truncated"); got+=n; }
                Check(zip.ReadByte()==-1,"BXML inflated length");
                uint a=1,c=0; foreach(byte v in b) { a=(a+v)%65521; c=(c+a)%65521; }
                Check((c<<16|a)==unchecked((uint)BE(source,source.Length-4)),"BXML checksum");
            }
            else { Check(version==0x3ea && source.Length-32==expected,"BXML version/length"); Buffer.BlockCopy(source,32,b,0,b.Length); }
            Range(b,0,pool);
            var names=Encoding.GetEncoding(28591).GetString(b,0,pool).Split('\0'); Check(names.Length==count+1 && names[count]=="","BXML strings");
            string Name(int i) { Check(i>=0 && i<count,"BXML string index"); return names[i]; }
            object Value(int i,int pooled,int type)
            {
                if(i==-1) return null;
                if(pooled==0 && type<=1) return Name(i);
                // This field is an element count, not a boolean: vegetation stores arrays here.
                if(pooled==0)return Array.Empty<float>();
                int components=type==8?16:type==10?3:type==7?4:type==9?2:1;
                int size=type==2?pooled:checked(components*4*pooled); Check(i>=0 && i<=poolSize-size,"BXML value"); int at=pool+i;
                if((type==5 && pooled>1) || type==7 || type==8 || type==9 || type==10)
                {
                    var a=new float[size/4];for(int j=0;j<a.Length;j++)a[j]=BitConverter.ToSingle(b,at+j*4);return a;
                }
                switch(type)
                {
                    case 2: return new ArraySegment<byte>(b,at,size);
                    case 3: return LE(b,at); case 4: return unchecked((uint)LE(b,at));
                    case 5: return BitConverter.ToSingle(b,at); case 11: return LE(b,at)!=0;
                    default: return null;
                }
            }
            var attrs=new KeyValuePair<string,object>[ac]; int p=pool+poolSize;
            for(int i=0;i<ac;i++,p+=12) attrs[i]=new KeyValuePair<string,object>(Name(LE(b,p)),Value(LE(b,p+4),BitConverter.ToUInt16(b,p+8),BitConverter.ToUInt16(b,p+10)));
            var nodes=new Node[nc]; for(int i=0;i<nc;i++) nodes[i]=new Node();
            var parented=new bool[nc];var depth=new int[nc];parented[0]=true;
            for(int i=0;i<nc;i++,p+=32)
            {
                var n=nodes[i]; n.tag=Name(LE(b,p)); n.value=Value(LE(b,p+4),LE(b,p+8),LE(b,p+12));
                int ch=LE(b,p+16), num=LE(b,p+20), attr=LE(b,p+24), na=LE(b,p+28);
                Check(num>=0 && num<=nc && (num==0 || ch>i && ch<=nc-num) && na>=0 && attr>=0 && attr<=ac-na,"BXML tree");
                for(int j=0;j<num;j++)
                {
                    Check(!parented[ch+j] && depth[i]<64,"BXML tree structure");parented[ch+j]=true;depth[ch+j]=depth[i]+1;n.children.Add(nodes[ch+j]);
                }
                for(int j=0;j<na;j++) n.attributes.Add(attrs[attr+j].Key,attrs[attr+j].Value);
            }
            Check(parented.All(v=>v),"BXML disconnected nodes");
            return nodes[0];
        }

        internal static List<Node> Scene(byte[] b,string activity="FreeRide")
        {
            Check(unchecked((uint)BE(b,0))==0xc0fffeee,"scene magic"); int count=BE(b,4); Check(count>0 && count<=64,"scene count");
            var entities=new Dictionary<int,Node>();
            // Some levels list Base last. Apply it first, then the selected activity's
            // overrides; disc table order must not replace course-specific placements.
            var order=Enumerable.Range(0,count).OrderBy(i=>Encoding.ASCII.GetString(b,8+i*264,256).TrimEnd('\0').EndsWith("Base",StringComparison.Ordinal)?0:1);
            foreach(int i in order)
            {
                int toc=8+i*264; Range(b,toc,264); string name=Encoding.ASCII.GetString(b,toc,256).TrimEnd('\0');
                if(!name.EndsWith("Base",StringComparison.Ordinal) && !name.EndsWith(activity,StringComparison.Ordinal)) continue;
                Check(BE(b,toc+256)==0,"scene offset"); int p=BE(b,toc+260), chunks=BE(b,p); p+=8;
                Check(chunks>=0 && chunks<=256,"scene chunks");
                for(int j=0;j<chunks;j++)
                {
                    int size=checked(LE(b,p+32)+36); Range(b,p,size); byte[] part=new byte[size]; Buffer.BlockCopy(b,p,part,0,size); p+=size;
                    foreach(var e in Xml(part).All().Where(n=>n.tag=="Entity"))
                    {
                        var n=e.children.FirstOrDefault(); if(n==null) continue;
                        var id=n.Property("Id")?.Leaf.value; if(id!=null) entities[Convert.ToInt32(id)]=n;
                    }
                }
            }
            Check(entities.Count>100,"scene entities"); return entities.Values.ToList();
        }
    }
}

// TEST DATA READER: bounded original NAT_Stillwater-nat TDF, no game state writes.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using Reflex.Port.Content;
using UnityEngine;

internal sealed class MotoAuthoredRoute
{
    public const string Digest = "7151f3397df4024ec02e4895296a98a234afd3b7d10f16546a710d5b59a3745c";
    readonly List<Vector3>[] sections = new List<Vector3>[6];
    readonly List<int>[] next = new List<int>[6];
    static uint U(byte[] b, int at) => ((uint)b[at] << 24) | ((uint)b[at+1] << 16) | ((uint)b[at+2] << 8) | b[at+3];
    static float F(byte[] b, int at) => BitConverter.ToSingle(BitConverter.GetBytes(U(b, at)), 0);
    static void Require(bool ok, string why) { if (!ok) throw new InvalidOperationException("National route: " + why); }
    public MotoAuthoredRoute()
    {
        byte[] data; var install = new Rfx.Install(ReflexInstall.DatabaseDir);
        try { data = install.Read("NAT_Stillwater", "NAT_StillwaterBase", "NAT_Stillwater-nat", "tdf"); }
        finally { install.Close(); }
        string digest; using (var sha = SHA256.Create()) digest = BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant();
        Require(digest == Digest && data.Length == 334332, "reviewed authored asset hash/size differs");
        //0053a060 removes the little-endian byte-count prefix.00536ef0 reads
        // four big-endian header counts. Known tag readers consume their actual
        // fields; declared vec3 length is16 although005395d0 consumes12 bytes.
        Require(BitConverter.ToUInt32(data,0) == data.Length-4 && U(data,4)==225 && U(data,8)==6 && U(data,12)==3 && U(data,16)==3, "header");
        int at=20, section=-1, nodes=0;
        while(at < data.Length)
        {
            Require(at+8 <= data.Length, "tag header extent");
            uint tag=U(data,at); int length=checked((int)U(data,at+4)); at+=8;
            switch(tag)
            {
                case 1: case 2: case 3: case 0x15: case 0x16: case 0x17: case 0x1a: case 0x2c: case 0x2d: length=12; break;
                case 4: case 5: case 7: case 0xd: case 0x11: case 0x18: case 0x19: case 0x1b: case 0x26: case 0x27: case 0x29: case 0x2a: case 0x1f: case 0xc: case 0xf: length=4; break;
                case 6: case 0x12: case 0x13: case 0x1c: case 0x1d: case 0x21: case 0x22: case 0x28: case 0x2e: case 0x2f: case 0x30: case 0x31:
                    Require(at+8 <= data.Length, "grid header"); length=checked(8+4*(int)U(data,at)*(int)U(data,at+4)); break;
                case 8: case 9: case 10: case 0x23: case 0x2b: length=48; break;
                case 0xb: case 0x14: length=0; break;
                case 0xe: case 0x10: length=8; break;
                case 0x24: Require(at+4<=data.Length,"array header"); length=checked(4+4*(int)U(data,at)); break;
            }
            Require(length >= 0 && length <= data.Length-at, "tag payload extent");
            if(tag==0xb) { Require(++section<6,"section count"); sections[section]=new List<Vector3>(); next[section]=new List<int>(); }
            if(tag==0x14) nodes++;
            if(tag==0x15) { Require(section>=0,"node before section"); sections[section].Add(new Vector3(F(data,at),0,F(data,at+8))); }
            if(tag==0xe) { Require(section>=0 && U(data,at)==next[section].Count,"next list index"); next[section].Add(checked((int)U(data,at+4))); }
            at+=length;
        }
        Require(at==data.Length && section==5 && nodes==225 && sections.Sum(s=>s.Count)==225,"complete parse");
        Require(sections[0].Count==87&&sections[1].Count==14&&sections[2].Count==94&&next[0].SequenceEqual(new[]{2})&&next[1].SequenceEqual(new[]{0})&&next[2].SequenceEqual(new[]{0}),"finite directed Stillwater route");
        foreach(int s in new[]{0,1,2})Require((sections[s].Last()-sections[next[s][0]][0]).sqrMagnitude==0,"exact authored directed join");
        Debug.Log("[moto-input-driver] AUTHORED sha256="+digest+" route=1->0->2->0 lapLength=4859.56689453125 startLength=1099.9952392578125");
    }

    // Verify native copied X/Z against the actual suffix (terrain loading changes
    // only authored Y=5001). A truncated64-node window is valid. A shorter window
    // must end at the original section end before following its authored next[0].
    public int Extend(Vector3[] points, int count, bool closed, out int section, out int node)
    {
        Require(!closed && count>0 && count<=64,"unexpected copied path shape");
        section=node=-1;var matches=new List<int[]>();
        for(int s=0;s<3;s++) for(int j=0;j<sections[s].Count;j++)
        {
            if(Math.Min(64,sections[s].Count-j)!=count) continue;
            bool same=true;
            for(int k=0;k<count;k++) if((points[k]-sections[s][j+k]).sqrMagnitude>.0001f){same=false;break;}
            if(same)matches.Add(new[]{s,j});
        }
        Require(matches.Count>0,"copied current path is not the reviewed main/start suffix");
        int successor;int previous;
        if(matches.Count==1){section=matches[0][0];node=matches[0][1];previous=Math.Min(node,4);successor=next[section][0];}
        else {
            // No guessed ancestry: the sole shared endpoint is valid only when
            // every full-suffix match ends there and has the SAME authored next.
            Require(count==1&&matches.All(m=>m[1]==sections[m[0]].Count-1),"ambiguous nonterminal suffix");
            var successors=matches.Select(m=>next[m[0]][0]).Distinct().ToArray();
            Require(successors.Length==1,"ambiguous authored successor");
            successor=successors[0];previous=0;section=node=-1;
        }
        // Genuine preceding authored nodes correct look-ahead-anchor projection.
        int kept=Math.Min(count,points.Length-previous);Array.Copy(points,0,points,previous,kept);
        for(int j=0;j<previous;j++)points[j]=sections[section][node-previous+j];count=previous+kept;
        int sNext=successor,jNext=1;
        while(count<points.Length){if(jNext>=sections[sNext].Count){sNext=next[sNext][0];jNext=1;}points[count++]=sections[sNext][jNext++];}
        return count;
    }
}

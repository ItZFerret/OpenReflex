using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
namespace Reflex.Port.Content
{
    public sealed class ExperimentalMtxAnimation : MonoBehaviour
    {
        sealed class Track { public Transform target; public float duration; public float[] qt,pt; public Quaternion[] rotations; public Vector3[] positions; }
        readonly List<Track> tracks=new List<Track>(); float started;
        static int Count(BinaryReader r) { int n=r.ReadInt32();if(n<1||n>10000)throw new InvalidDataException("Invalid animation key count");return n; }
        static float F(BinaryReader r){float v=r.ReadSingle();if(!float.IsFinite(v))throw new InvalidDataException("Invalid animation value");return v;}
        public static void Read(BinaryReader r,GameObject host,Transform[] groups)
        {
            int clips=r.ReadInt32();if(clips<0||clips>1000)throw new InvalidDataException("Invalid clip count");
            if(clips==0)return;
            var anim=host.AddComponent<ExperimentalMtxAnimation>();anim.started=Time.time;
            for(int i=0;i<clips;i++)
            {
                float duration=F(r);if(duration<=0||duration>86400)throw new InvalidDataException("Invalid clip duration");
                int count=Count(r);
                for(int j=0;j<count;j++)
                {
                    int group=r.ReadInt32();if(group<0||group>=groups.Length)throw new InvalidDataException("Invalid animated group");
                    var track=new Track{target=groups[group],duration=duration};int nq=Count(r);
                    track.qt=new float[nq];track.rotations=new Quaternion[nq];
                    for(int k=0;k<nq;k++){track.qt[k]=F(r);track.rotations[k]=new Quaternion(F(r),F(r),F(r),F(r));if(k>0&&track.qt[k]<track.qt[k-1])throw new InvalidDataException("Unsorted rotation keys");}
                    int nt=Count(r);track.pt=new float[nt];track.positions=new Vector3[nt];
                    for(int k=0;k<nt;k++){track.pt[k]=F(r);track.positions[k]=new Vector3(F(r),F(r),F(r));if(k>0&&track.pt[k]<track.pt[k-1])throw new InvalidDataException("Unsorted position keys");}
                    if(track.target.TryGetComponent<ExperimentalMtxSpinner>(out var spinner)) { spinner.enabled=false;Destroy(spinner); }
                    anim.tracks.Add(track);
                }
            }
            anim.Pose(0);Debug.Log($"[MTX] animated scenery: {anim.tracks.Count} object tracks");
        }
        static int Key(float[] times,float t){int k=Array.BinarySearch(times,t);return Mathf.Clamp(k<0?~k-1:k,0,times.Length-1);}
        void LateUpdate()=>Pose(Time.time-started);
        void Pose(float elapsed)
        {
            foreach(var t in tracks)
            {
                if(!t.target)continue;float at=elapsed%t.duration;int p=Key(t.pt,at),q=Key(t.qt,at);
                t.target.localPosition=p+1<t.pt.Length?Vector3.Lerp(t.positions[p],t.positions[p+1],Mathf.InverseLerp(t.pt[p],t.pt[p+1],at)):t.positions[p];
                t.target.localRotation=q+1<t.qt.Length?Quaternion.Slerp(t.rotations[q],t.rotations[q+1],Mathf.InverseLerp(t.qt[q],t.qt[q+1],at)):t.rotations[q];
            }
        }
    }
}

// TEST ONLY heuristic. Actual copied route/pose in, ordinary mapped keys out.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;

internal sealed class MotoInputDriver
{
    const string Tag="[moto-input-driver] ";
    readonly MotoAuthoredRoute route = new MotoAuthoredRoute();
    readonly Vector3[] line = new Vector3[64];
    readonly int throttle,brake,left,right;
    OriginalWorldRuntime owner;
    bool attached;
    int count,section,node,lastLap=-1;
    uint lineFrame,frame;
    double changedAt,lastLog,lineMissingSince=-1;
    float bestProgress,progressClock;
    string lineError;
    // exhibition: the arcade National at NAT_Stillwater (ArcadeReplayFlowProbe), no career; the same route and steering policy.
    readonly bool exhibition;
    public MotoInputDriver(RemapTable controls, bool exhibition = false)
    {
        this.exhibition=exhibition;
        throttle=Scan(controls,"Throttle");brake=Scan(controls,"Brake");left=Scan(controls,"SteerLeft");right=Scan(controls,"SteerRight");
        if(new[]{throttle,brake,left,right}.Distinct().Count()!=4) throw new InvalidOperationException("National test input requires distinct keyboard actions.");
        Debug.Log(Tag+"BOUND throttle="+throttle+" brake="+brake+" left="+left+" right="+right);
    }
    static int Scan(RemapTable t,string name)
    {
        if(t==null)throw new InvalidOperationException("No copied controls.");
        int col=Array.IndexOf(t.columns,name);
        if(col<0||col>=t.kb.Length)throw new InvalidOperationException("Missing keyboard action "+name);
        var matches=Enumerable.Range(1,255).Where(i=>t.scan[i]==t.kb[col]).ToArray();
        if(t.kb[col]==0||matches.Length!=1)throw new InvalidOperationException("Nonunique scan code for "+name);
        return matches[0];
    }
    static bool Finite(float x)=>!float.IsNaN(x)&&!float.IsInfinity(x);
    public const string ExhibitionEvent="National/NAT_Stillwater#";
    bool Owned(OriginalWorldRuntime r)=>exhibition?!r.OwnedCareerActive&&r.EventIdentity!=null&&r.EventIdentity.StartsWith(ExhibitionEvent):r.OwnedCareerActive;
    bool RiderCount(int n)=>exhibition?n>=1&&n<=16:n==12;
    static string F(float x)=>x.ToString("R",CultureInfo.InvariantCulture);
    static string V(Vector3 p)=>F(p.x)+","+F(p.y)+","+F(p.z);
    static Vector3 Vec(float[] v)
    {
        if(v==null||v.Length<3||!Finite(v[0])||!Finite(v[1])||!Finite(v[2]))throw new InvalidOperationException("Invalid copied bike vector.");
        return new Vector3(v[0],v[1],v[2]);
    }
    public void Attach(OriginalWorldRuntime runtime)
    {
        if(attached){if(!ReferenceEquals(owner,runtime))throw new InvalidOperationException("National input cannot rebind another world.");return;}
        if(runtime.testRaceLineObserver!=null)throw new InvalidOperationException("Copied line observer already owned.");
        owner=runtime;attached=true;runtime.testRaceLineObserver=Capture;
    }
    void Capture(float[] values,int points,bool closed,uint copiedFrame)
    {
        // Borrowed presentation buffer never escapes this synchronous callback.
        // Runtime mutation is limited to installing this test observer; no native call.
        lineFrame=copiedFrame;count=0;lineError=null;
        if(points==0)return;
        try
        {
            if(values==null||points<1||points>64||values.Length<points*3)throw new InvalidOperationException("Invalid copied line extent.");
            for(int i=0;i<points;i++)
            {
                float x=values[3*i],z=values[3*i+2];
                if(!Finite(x)||!Finite(z))throw new InvalidOperationException("Nonfinite copied line.");
                line[i]=new Vector3(x,0,z);
            }
            count=route.Extend(line,points,closed,out section,out node);
        }
        catch(Exception e){lineError=e.Message;count=0;}
    }
    Vector3 Ahead(int segment,Vector3 from,float distance)
    {
        for(int i=segment+1;i<count;i++)
        {
            float length=Vector3.Distance(from,line[i]);
            if(length>=distance&&length>.0001f)return Vector3.Lerp(from,line[i],distance/length);
            distance-=length;from=line[i];
        }
        return from;
    }
    int observedLap=-1;public bool ThreeLapsObserved=>observedLap==ExpectedLaps;
    // REFLEX_TEST_LAPS (run.py --laps): the original's debug lap override shortens the motos for iteration; default the authored 3.
    public static readonly int ExpectedLaps=int.TryParse(Environment.GetEnvironmentVariable("REFLEX_TEST_LAPS"),out var testLaps)&&testLaps>0?testLaps:3;
    ulong observationGeneration;uint observationPhase,observationSub;bool observationIdentity;
    long observationWorld;string observationEvent;
    public void Dispose(){if(owner!=null&&owner.testRaceLineObserver==Capture)owner.testRaceLineObserver=null;}
    // READ ONLY: final lap may arrive in natural WindDown/Post with queued UI requests.
    // This method never holds input, posts receipts or writes any runtime/native state.
    public void Observe(OriginalWorldRuntime runtime,bool genuineIdentity,ulong generation,uint phase,uint subEvent)
    {
        if(!genuineIdentity||!runtime.Running||!Owned(runtime))return;
        var race=runtime.Race;var bike=runtime.Bike;
        if(race.valid==0)return;
        // The original's SubEvent (MXSeriesEventMoto+4) stays 0 in both motos; phase 6 / 7 is the moto.
        if(!exhibition&&(phase!=6&&phase!=7||subEvent!=0u))return;
        if((!exhibition&&runtime.EventIdentity!="Career/14#"+runtime.WorldGeneration)||runtime.TestCopiedRaceIdentity!=runtime.EventIdentity||runtime.TestCopiedRaceWorld!=runtime.WorldGeneration)return;
        // The published race is from done.bike.frames, not inferred from a blended pose.
        // A post frame can have no route; its race/finite-pose witness still comes from one completed packet.
        if(!runtime.TestCopiedRacePoseFinite||runtime.TestCopiedRaceFrame!=runtime.TestCopiedRacePoseFrame)return;
        if(runtime.IsWaypoint||race.num_laps!=ExpectedLaps||!RiderCount(race.riders)||race.laps_completed<-1||race.laps_completed>ExpectedLaps||!Finite(race.race_time)||!Finite(race.lap_progress))throw new InvalidOperationException("Copied Moto race identity/extent differs.");
        if(race.state!=2&&race.state!=3&&race.state!=4&&race.state!=5)return;
        if(!observationIdentity){
            if(race.state!=2||race.laps_completed!=0||count<2||lineError!=null||runtime.TestCopiedRaceFrame!=bike.frames||lineFrame!=bike.frames)return;
            observationIdentity=true;observationGeneration=generation;observationPhase=phase;observationSub=subEvent;observationWorld=runtime.WorldGeneration;observationEvent=runtime.EventIdentity;observedLap=0;
            Debug.Log(Tag+"READONLY_LAP lap=0 phase="+phase+" sub="+subEvent+" generation="+generation+" world="+observationWorld+" frame="+runtime.TestCopiedRaceFrame+" state="+race.state+" armedRacing0=1");
        }else{
            if(generation!=observationGeneration||phase!=observationPhase||subEvent!=observationSub||runtime.WorldGeneration!=observationWorld||runtime.EventIdentity!=observationEvent)throw new InvalidOperationException("Current Moto readonly observation identity changed.");
            if(race.laps_completed!=observedLap){
                if(race.laps_completed!=observedLap+1)throw new InvalidOperationException("Current Moto natural lap sequence skipped or regressed.");
                observedLap=race.laps_completed;
                Debug.Log(Tag+"READONLY_LAP lap="+observedLap+" phase="+phase+" sub="+subEvent+" generation="+generation+" world="+observationWorld+" frame="+runtime.TestCopiedRaceFrame+" state="+race.state+" inputIndependent=1");
            }
        }
    }
    public int[] Inputs(OriginalWorldRuntime runtime,bool authorized,double now,ulong generation,uint phase,uint subEvent)
    {
        var race=runtime.Race;var selector=runtime.GetComponent<ReflexEventSelect>();
        if(!authorized||!runtime.Running||!Owned(runtime)||runtime.paused||runtime.OwnMenuActive||
            (runtime.optionsMenu&&runtime.optionsMenu.IsOpen)||(runtime.pauseMenu&&runtime.pauseMenu.IsOpen)||(selector&&selector.IsOpen)||race.valid==0||race.state!=2||race.laps_completed>=race.num_laps)
        {changedAt=now;lineMissingSince=-1;return Array.Empty<int>();}
        if(runtime.IsWaypoint||race.num_laps!=ExpectedLaps||!RiderCount(race.riders)||race.laps_completed<-1||!Finite(race.race_time)||!Finite(race.lap_progress))throw new InvalidOperationException("Copied race differs from finite National Series14.");
        var bike=runtime.Bike;
        if(bike.frames!=frame){frame=bike.frames;changedAt=now;}
        if(now-changedAt>3)throw new TimeoutException("Copied bike stopped advancing while racing.");
        if(lineError!=null)throw new InvalidOperationException(lineError);
        if(count<2||lineFrame!=bike.frames)
        {
            if(lineMissingSince<0)lineMissingSince=now;
            if(now-lineMissingSince>3)throw new TimeoutException("No matching copied race/route/pose frame while racing.");
            return Array.Empty<int>();
        }
        lineMissingSince=-1;
        bool lapChanged=race.laps_completed!=lastLap;
        if(lapChanged){lastLap=race.laps_completed;bestProgress=race.lap_progress;progressClock=race.race_time;Debug.Log(Tag+"ACTUAL_LAP frame="+frame+" lap="+lastLap+" progress="+F(race.lap_progress));}
        if(race.lap_progress>bestProgress+.005f){bestProgress=race.lap_progress;progressClock=race.race_time;}
        if(race.race_time-progressClock>100)throw new TimeoutException("No forward copied lap progress for100 race seconds; review input policy.");
        Vector3 position=Vec(bike.position);position.y=0;
        Vector3 velocity=Vec(bike.velocity);velocity.y=0;
        var raw=bike.orientation;
        if(raw==null||raw.Length<4||raw.Take(4).Any(x=>!Finite(x)))throw new InvalidOperationException("Invalid copied orientation.");
        float norm=Mathf.Sqrt(raw[0]*raw[0]+raw[1]*raw[1]+raw[2]*raw[2]+raw[3]*raw[3]);
        if(norm<.5f||norm>1.5f)throw new InvalidOperationException("Invalid copied orientation norm.");
        var q=new Quaternion(raw[0]/norm,raw[1]/norm,raw[2]/norm,raw[3]/norm);
        Vector3 forward=q*Vector3.forward;forward.y=0;
        float speed=velocity.magnitude;
        // D+1c anchors the local directed route. Limit projection to the nearby
        // six segments, rather than selecting a shortcut on another course arm.
        int segment=0;Vector3 projected=line[0];float nearest=float.MaxValue;
        for(int i=0;i<Math.Min(count-1,6);i++)
        {
            Vector3 edge=line[i+1]-line[i];float d=edge.sqrMagnitude;if(d<.0001f)continue;
            Vector3 point=line[i]+edge*Mathf.Clamp01(Vector3.Dot(position-line[i],edge)/d);
            float ds=(position-point).sqrMagnitude;if(ds<nearest){nearest=ds;projected=point;segment=i;}
        }
        float crossTrack=Mathf.Sqrt(nearest);
        float look=Mathf.Clamp(15+speed*.65f,25,65);
        if(crossTrack>35)look=15;
        Vector3 target=Ahead(segment,projected,look),far=Ahead(segment,projected,look+45);
        Vector3 delta=target-position;
        float angle=forward.sqrMagnitude<.01f||delta.sqrMagnitude<.01f?0:Vector3.SignedAngle(forward,delta,Vector3.up);
        float corner=Vector3.Angle(target-projected,far-target);
        if(!Finite(speed)||!Finite(angle)||!Finite(crossTrack)||!Finite(corner))throw new InvalidOperationException("Nonfinite route steering.");
        float turn=Mathf.Abs(angle);
        float desired=turn>90?12:turn>50?24:turn>25?38:68;
        desired=Mathf.Min(desired,corner>55?24:corner>25?40:68);
        if(crossTrack>35)desired=Mathf.Min(desired,24);
        bool upright=Vector3.Dot(q*Vector3.up,Vector3.up)>.35f;
        var keys=new List<int>(2);
        if(upright&&forward.sqrMagnitude>.01f)
        {
            if(speed<desired-2)keys.Add(throttle);else if(speed>desired+4)keys.Add(brake);
            float duty=Mathf.Clamp(turn/30,.15f,1);
            if(turn>2.5f&&Mathf.Repeat(race.race_time,.2f)<.2f*duty)keys.Add(angle>0?right:left);
        }
        if(lapChanged||now-lastLog>=1)
        {
            lastLog=now;Debug.Log(Tag+"INPUT frame="+frame+" clock="+F(race.race_time)+" lap="+lastLap+" progress="+F(race.lap_progress)+
                " section="+section+" node="+node+" position="+V(position)+" target="+V(target)+" headingError="+F(angle)+" crossTrack="+F(crossTrack)+
                " speed="+F(speed)+" desiredSpeed="+F(desired)+" corner="+F(corner)+" upright="+upright+" scans="+string.Join(",",keys));
        }
        return keys.ToArray();
    }
}

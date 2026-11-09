// Open Unleashed Dynamic FPV presentation, adapted to Reflex's authored rider eye nodes.
// The spring/filter/view equations follow MxaOnboardCamera and its Machine partial;
// only skeleton calibration and available Reflex state differ. Native game physics is untouched.
//
// w-fpvrevert (2026-11-03): this camera again, after w-fpvrewrite's head-on-a-neck model (0577e9f3 .. af48baf1) was disliked by the playtesters.
// The motion model is the 7a9c99ff camera's (tests/menus_showcase/fixture/FpvOldCamera.cs is that file: the reference of FpvRideProbe's
// REFLEX_FPV_COMPARE). Kept from the rewrite era, none of them about the feel:
//  - the settings are fields Unity never serializes ([NonSerialized]: no scene value stands in for the player's), read from the ReflexFpv.* keys
//    (ReflexSettings.Onboard*) with this file's defaults as the fallback, and read again whenever one of those keys changes (ReflexSave.Changed:
//    an options row, a share code, a restored backup, RESET deleting them), not only on ReflexClientLive.FpvChanged
//  - a new world (NewSession) or another vehicle kind starts the springs, the machine seat and the head-gaze calibration again
//  - a fallback to the game's camera that lasts is logged once with its reason ("[FPV] fallback: ..."); FallbackText for the options page
//  - the cockpit rider hides the upper arms as well: never in the default view (no elbow in the frustum on ~1900 frames of bike / ATV rides), but
//    in it as soon as the eye moves back (EYE FORWARD / BACK, the eye pushed 0.3 ft behind the hands: an elbow on 8 % of the frames). Not kept: the
//    rewrite's "eye behind the hands" (6b9b453e): this camera's eye rides at the rider's own goggles, which are over the bars (median 0.3 ft ahead of
//    the wrists on the bike, 0.8 ft on the ATV): moving it 0.3 ft behind them moved the default view ~0.6 ft back on every frame (the bars in view: another look); EYE FORWARD /
//    BACK does that for a player who wants it
//  - REFLEX_FPV_CAMLOG=<csv>: the internals per frame
// w-fpvrevert's small changes to the feel (each a ReflexFpv.* key with an options row; the defaults and what they change: core/plan/QUEUE.md
// w-fpvrevert): HEAD BOB (headBob: how much of the animated head's movement the eye takes, 0 = none), HEAD SMOOTHING (headSmoothing: the eye
// spring), VIEW SMOOTHING (viewSmoothing: a critically damped spring on the view frame's up, i.e. its pitch and roll; 0 = none, the old camera).
using System;
using System.Globalization;
using System.IO;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    [DisallowMultipleComponent]
    public sealed class OriginalOnboardCamera : MonoBehaviour
    {
        public const int HeadModeChassis = 0, HeadModeDynamic = 1, HeadModeTrue = 2;
        const float Ft = 0.3048f, Step = 0.002f;
        const int HeadBone = 31, REyeBone = 32, LEyeBone = 33;
        const float TorsoLen = 0.65f / Ft;
        // Reflex's measured neutral riding eye, relative to body zero, not the Unleashed body origin.
        static readonly Vector3 TorsoPivot = new Vector3(0f, 1.65f, -0.03f);
        public const float DefaultFov = 116f, DefaultTilt = 0f, DefaultPitch = 0.31f,
                           DefaultCaScale = 0.5f, DefaultLeanHeadingScale = 35.2f;
        /// w-fpvrevert's feel settings: HEAD BOB (1 = the old camera's), HEAD SMOOTHING (s; the old camera's 0.035, now 0.05), VIEW SMOOTHING (s; the old
        /// camera's 0 = none, now 0.03): a little less of the head's and the ground's chatter, measured in core/plan/QUEUE.md w-fpvrevert.
        public const float DefaultHeadBob = 1f, DefaultHeadSmoothing = 0.05f, DefaultViewSmoothing = 0.03f;
        // [NonSerialized] (w-fpvrevert): the scenes carry serialized values of these fields, which Unity applied before Load, so a scene's value, not this
        // file's default, was the fallback of an unset key (the trap of af48baf1's configOverride). Code defaults + the player's keys only.
        [NonSerialized] public int headMode = HeadModeDynamic;
        [NonSerialized] public float fov = DefaultFov;
        [NonSerialized] public float tilt = DefaultTilt;
        [NonSerialized] public float pitch = DefaultPitch;
        [NonSerialized] public bool leanPitch = true;
        [NonSerialized] public int cornerAnticipation;
        [NonSerialized] public float cornerAnticipationScale = DefaultCaScale;
        [NonSerialized] public float leanHeadingScale = DefaultLeanHeadingScale;
        [NonSerialized] public bool freeLook, headShift;
        [NonSerialized] public float shiftLong = 1f, shiftLat = 1f, shiftVert = 1f;
        [NonSerialized] public float trueSmoothing = 0.35f, trueRoll = 1f, trueYawSmoothing, truePitchSmoothing;
        [NonSerialized] public Vector3 offset;
        [NonSerialized] public float headBob = DefaultHeadBob, viewSmoothing = DefaultViewSmoothing;
        public const float OffsetMax = 2f;
        [NonSerialized] public float lensForward = 0.08f, headSmoothing = DefaultHeadSmoothing, headLimitUp = 1.2f, nearPlane = 0.1f;
        [NonSerialized] public Vector3 headLimit = new Vector3(0.6f,0.8f,0.7f);
        /// Tests (FpvRideProbe's comparison cameras): settings set in code; Load / Save and the key watch leave them alone.
        [NonSerialized] public bool ownSettings;
        public bool Active { get; private set; }
        public float LeanDeg { get; private set; }
        public float LookYaw => yaw;
        public float PitchDeg { get; private set; }
        public float RollDeg { get; private set; }
        public float CornerTarget { get; private set; }
        public float TrickWeight => trickWeight;
        public int HeadMode { get; private set; } = HeadModeDynamic;
        public Vector3 RawEyeRel { get; private set; }
        public Vector3 EyeRel { get; private set; }
        public Vector3 EyeInHead { get; private set; }
        public Vector3 HeadShift { get; private set; }
        public Vector3 Accel => accBox.Out;
        public string FallbackReason { get; private set; } = "";
        public float RawLeanForward { get; private set; }
        public float LeanForward => filteredLeanForward;
        public Vector3 TravelForward { get; private set; }
        public float TravelSpeedMetres { get; private set; }
        public float TravelBlend { get; private set; }
        // Source MxaBikeState.leanInput is already rate-limited by MXVehicleMotoShared::vf36.
        // Reflex's exported S+38 is the raw action, so reproduce that presentation input contract here.
        const float SourceLeanRate = 3.5f;
        float filteredLeanForward;
        bool leanPrimed;
        Camera cam;
        OriginalRider rider;
        readonly Box upBox = new Box(0.5f), velBox = new Box(0.1f), accBox = new Box(0.2f);
        Vector3 upSrc, velSrc, accSrc, prevVel, lastPos, eyeRel, eyeVel;
        float prevElapsed, stepAcc, yaw, caYaw, caYawVel, mouseYaw, mousePitch, mouseIdle, pendingDt, trickWeight;
        bool haveVel, primed, eyePrimed, yawPrimed, truePrimed, seatSet;
        Vector3 trueEye, trueVel, seatEye;
        Quaternion trueRot, gazeCalibration;
        bool gazeCalibrated;
        int lastFrame = -1;
        double clock;
        float[] raceLine = new float[0]; int raceLinePoints; bool raceLineClosed;
        bool isMachine, isAtv, riderActive = true, wrecked, trick, rightStickRiding = true;
        bool frontGrounded, rearGrounded, haveContacts, resetPending;
        double renderClock = double.NaN;
        const string P = "ReflexFpv.";

        public void Bind(Camera camera, OriginalRider riderModel)
        {
            if (rider != riderModel) { Deactivate(); ReleaseCockpit(rider); gazeCalibrated = false; Resnap(); }
            cam = camera; rider = riderModel;
        }
        // Context is read-only game state. Clock must refer to the rendered pose (NaN: completed-frame dt fallback).
        public void SetContext(bool isAtv, bool isMachine, bool riderActive, bool wrecked, bool trick,
                               bool rightStickRiding, bool frontGrounded, bool rearGrounded, bool haveContacts,
                               double renderClock, bool reset = false)
        {
            // A wreck can finish beside the same bike position: distance-only teleport detection misses it.
            // Drop old spring/filter time on loss and recovery so the next seated pose starts at dt0.
            if (this.isMachine != isMachine || (this.wrecked || !this.riderActive) != (wrecked || !riderActive)) Resnap();
            if (this.isAtv != isAtv) Resnap();   // (w-fpvrevert) another vehicle kind: the springs and its seat start again
            this.isAtv = isAtv;
            this.isMachine = isMachine; this.riderActive = riderActive; this.wrecked = wrecked; this.trick = trick;
            this.rightStickRiding = rightStickRiding; this.frontGrounded = frontGrounded;
            this.rearGrounded = rearGrounded; this.haveContacts = haveContacts; this.renderClock = renderClock;
            resetPending |= reset && (double.IsNaN(renderClock) || renderClock != clock);
            // ATV uses the actual Reflex eye bones; the source game's .65ft body-origin difference is not copied.
        }
        void Awake() { Load(); }
        // (w-fpvrevert) any writer of a ReflexFpv.* key (the options rows, a share code, RESET, a restored backup) reaches the running camera: the next
        // Apply reads the settings again
        bool settingsDirty;
        void OnEnable() { ReflexSave.Changed += OnSaveChanged; }
        void OnSaveChanged(string key) { if (key != null && key.StartsWith(P, StringComparison.Ordinal)) settingsDirty = true; }
        /// The player's settings (the ReflexFpv.* keys, ReflexSettings.Onboard*; an unset key = this file's default). The 7a9c99ff camera read the same
        /// keys from PlayerPrefs with the current field as the fallback, so after RESET (the keys deleted) a running camera kept the old values.
        public void Load()
        {
            settingsDirty = false;
            if (ownSettings) return;
            ReflexSettings.MigrateFpvConfig();
            headMode = ReflexSettings.OnboardHeadMode;
            fov = ReflexSettings.OnboardFov;
            tilt = ReflexSettings.OnboardTilt;
            pitch = ReflexSettings.OnboardPitch;
            leanPitch = ReflexSettings.OnboardLeanPitch;
            cornerAnticipation = ReflexSettings.OnboardCornerAnticipation;
            cornerAnticipationScale = ReflexSettings.OnboardCornerScale;
            leanHeadingScale = ReflexSettings.OnboardLeanHeading;
            freeLook = ReflexSettings.OnboardFreeLook;
            headShift = ReflexSettings.OnboardHeadShift;
            shiftLong = ReflexSettings.OnboardShiftLong; shiftLat = ReflexSettings.OnboardShiftLat; shiftVert = ReflexSettings.OnboardShiftVert;
            trueSmoothing = ReflexSettings.OnboardTrueSmoothing; trueRoll = ReflexSettings.OnboardTrueRoll;
            trueYawSmoothing = ReflexSettings.OnboardTrueYawSmoothing; truePitchSmoothing = ReflexSettings.OnboardTruePitchSmoothing;
            offset = ClampOffset(ReflexSettings.OnboardOffset);
            headBob = ReflexSettings.OnboardHeadBob; headSmoothing = ReflexSettings.OnboardHeadSmoothing; viewSmoothing = ReflexSettings.OnboardViewSmoothing;
            settingsDirty = false;   // (the migration's own writes)
        }
        public void Save()
        {
            if (ownSettings) return;
            ReflexSettings.OnboardHeadMode = headMode; ReflexSettings.OnboardFov = fov; ReflexSettings.OnboardTilt = tilt; ReflexSettings.OnboardPitch = pitch;
            ReflexSettings.OnboardLeanPitch = leanPitch; ReflexSettings.OnboardCornerAnticipation = cornerAnticipation;
            ReflexSettings.OnboardCornerScale = cornerAnticipationScale; ReflexSettings.OnboardLeanHeading = leanHeadingScale;
            ReflexSettings.OnboardFreeLook = freeLook; ReflexSettings.OnboardHeadShift = headShift;
            ReflexSettings.OnboardShiftLong = shiftLong; ReflexSettings.OnboardShiftLat = shiftLat; ReflexSettings.OnboardShiftVert = shiftVert;
            ReflexSettings.OnboardTrueSmoothing = trueSmoothing; ReflexSettings.OnboardTrueRoll = trueRoll;
            ReflexSettings.OnboardTrueYawSmoothing = trueYawSmoothing; ReflexSettings.OnboardTruePitchSmoothing = truePitchSmoothing;
            offset = ClampOffset(offset); ReflexSettings.SetOnboardOffset(offset, false);
            ReflexSettings.OnboardHeadBob = headBob; ReflexSettings.OnboardHeadSmoothing = headSmoothing; ReflexSettings.OnboardViewSmoothing = viewSmoothing;
            ReflexSettings.Save();
            settingsDirty = false;
        }
        public static Vector3 ClampOffset(Vector3 v) => new Vector3(Mathf.Clamp(v.x,-OffsetMax,OffsetMax),Mathf.Clamp(v.y,-OffsetMax,OffsetMax),Mathf.Clamp(v.z,-OffsetMax,OffsetMax));
        public void ResetDefaults()
        {
            headMode=HeadModeDynamic; fov=DefaultFov; tilt=DefaultTilt; pitch=DefaultPitch; leanPitch=true;
            cornerAnticipation=0; cornerAnticipationScale=DefaultCaScale; leanHeadingScale=DefaultLeanHeadingScale;
            freeLook=headShift=false; shiftLong=shiftLat=shiftVert=1; trueSmoothing=.35f; trueRoll=1;
            trueYawSmoothing=truePitchSmoothing=0; offset=Vector3.zero;
            headBob=DefaultHeadBob; headSmoothing=DefaultHeadSmoothing; viewSmoothing=DefaultViewSmoothing; Save();
        }
        public string SettingsText => $"FPV {(headMode==HeadModeTrue?"TRUE":headMode==HeadModeDynamic?"DYNAMIC":"CHASSIS")}  FOV {fov:0}°  CORNER {(cornerAnticipation==0?"OFF":cornerAnticipation==1?"TRACK":"LEAN")}{(freeLook?"  FREE LOOK":"")}";
        public void HandleKeys()
        {
            var kb=Keyboard.current; if(kb==null)return; bool changed=false;
            if(!OriginalInput.IsCameraKey(Key.F8)&&kb.f8Key.wasPressedThisFrame){headMode=(headMode+1)%3;changed=true;}
            if(!kb.ctrlKey.isPressed&&!OriginalInput.IsCameraKey(Key.F9)&&kb.f9Key.wasPressedThisFrame){fov=Mathf.Max(40,fov-2);changed=true;}
            if(!kb.ctrlKey.isPressed&&!OriginalInput.IsCameraKey(Key.F10)&&kb.f10Key.wasPressedThisFrame){fov=Mathf.Min(140,fov+2);changed=true;}
            if(!kb.ctrlKey.isPressed&&!OriginalInput.IsCameraKey(Key.F11)&&kb.f11Key.wasPressedThisFrame){cornerAnticipation=(cornerAnticipation+1)%3;changed=true;}
            if(!OriginalInput.IsCameraKey(Key.F12)&&kb.f12Key.wasPressedThisFrame){freeLook=!freeLook;changed=true;}
            if(changed)Save();
        }
        public static float VerticalFov(float fovDeg,float aspect)
        {
            float t=Mathf.Tan(Mathf.Clamp(fovDeg,1,179)*.5f*Mathf.Deg2Rad);
            if(!(aspect>0))aspect=4f/3f;
            return 2*Mathf.Atan(t/Mathf.Clamp(aspect,4f/3f,16f/9f))*Mathf.Rad2Deg;   // wider than 16:9 (ultrawide FILL SCREEN): the 16:9 vertical angle, the extra width added at the sides
        }
        public void OnSimFrame(float dt,float[] line,int points,bool lineClosed=false)
        {
            if(!float.IsNaN(dt)&&!float.IsInfinity(dt)&&dt>0)pendingDt+=dt;
            if(line!=null){raceLine=line;raceLinePoints=Mathf.Clamp(points,0,line.Length/3);raceLineClosed=lineClosed;}
        }
        /// A new world (its vehicle and rider model may be others): the springs, the machine seat and the head-gaze calibration start again.
        public void NewSession() { gazeCalibrated = false; Resnap(); }
        public void Resnap()
        {
            primed=eyePrimed=yawPrimed=truePrimed=seatSet=leanPrimed=viewPrimed=false;
            yaw=caYaw=caYawVel=mouseYaw=mousePitch=mouseIdle=trickWeight=pendingDt=0;
            eyeVel=trueVel=Vector3.zero; haveVel=false; lastFrame=-1;
        }
        public void Deactivate()
        {
            if(Active&&rider)SetCockpit(rider,false);
            Active=false;
            // Missing/invalid authored pose also suspends this view, without a head-position veto.
            // Never integrate the whole time spent in an external camera through the eye spring on recovery.
            Resnap();
        }
        void OnDisable(){ReflexSave.Changed -= OnSaveChanged;Deactivate();}
        void OnDestroy(){ReflexSave.Changed -= OnSaveChanged;Deactivate();ReleaseCockpit(rider);camLog?.Dispose();camLog=null;}
        static bool Finite(Vector3 v) => !(float.IsNaN(v.x)||float.IsNaN(v.y)||float.IsNaN(v.z)||float.IsInfinity(v.x)||float.IsInfinity(v.y)||float.IsInfinity(v.z));

        // Reflex bone axes are not the source game's canonical head axes. Recover that basis once from the bind skeleton.
        Quaternion HeadGaze(Transform head,Vector3 eye,Quaternion bodyRot)
        {
            if(!gazeCalibrated)
            {
                Quaternion neutralHead=head.rotation; Vector3 direction=eye-head.position,up=bodyRot*Vector3.up;
                var bind=rider.bindWorldMatrices;
                if(bind!=null&&bind.Length>LEyeBone*16+14)
                {
                    Vector3 hp=BindPosition(bind,HeadBone),ep=(BindPosition(bind,REyeBone)+BindPosition(bind,LEyeBone))*.5f;
                    Vector3 hf=BindRow(bind,HeadBone,8),hu=BindRow(bind,HeadBone,4);
                    if(hf.sqrMagnitude>1e-8f&&hu.sqrMagnitude>1e-8f){neutralHead=Quaternion.LookRotation(hf,hu);direction=ep-hp;up=Vector3.up;}
                }
                direction=Vector3.ProjectOnPlane(direction,up);
                if(direction.sqrMagnitude<1e-8f)direction=bodyRot*Vector3.forward;
                gazeCalibration=Quaternion.Inverse(neutralHead)*Quaternion.LookRotation(direction,up);
                gazeCalibrated=true;
            }
            return head.rotation*gazeCalibration;
        }
        static Vector3 BindPosition(float[] b,int node)=>BindRow(b,node,12);
        static Vector3 BindRow(float[] b,int node,int row){int o=node*16+row;return new Vector3(b[o],b[o+1],b[o+2]);}

        public bool Apply(Vector3 bodyPos,Quaternion bodyRot,Vector3 front,Vector3 rear,Vector3 vel,float speed,
                          float leanFb,float leanLr,Terrain terrain,bool lookBack)
        {
            if(settingsDirty)Load();
            if(!cam)return Reject("camera-missing");
            if(!rider)return Reject("rider-missing");
            if(!riderActive)return Reject("rider-inactive");
            if(wrecked)return Reject("wrecked");
            if(!rider.HasOriginalPose)return Reject("authored-pose-missing");
            if(rider.bones==null||rider.bones.Length<=LEyeBone)return Reject("eye-bones-missing");
            if(!Finite(bodyPos)||!Finite(vel))return Reject("body-nonfinite");
            var head=rider.bones[HeadBone];var re=rider.bones[REyeBone];var le=rider.bones[LEyeBone];
            if(!head||!re||!le)return Reject("eye-bone-transform-missing");
            Vector3 lenses=(re.position+le.position)*.5f;
            Quaternion gaze=HeadGaze(head,lenses,bodyRot);
            Vector3 actualEye=lenses+gaze*Vector3.forward*lensForward;
            if(!Finite(actualEye))return Reject("eye-nonfinite");
            float dt=UpdateFilters(bodyPos,bodyRot,front,rear,vel,terrain);
            leanFb=FilterLeanForward(leanFb,dt);leanLr=Mathf.Clamp(leanLr,-1,1);HeadMode=headMode;
            if(isMachine)return ApplyMachine(head,actualEye,bodyPos,bodyRot,dt,lookBack);
            if(headMode==HeadModeTrue)return ApplyTrue(head,actualEye,gaze,bodyPos,bodyRot,vel,leanFb,dt,lookBack);
            truePrimed=false;
            Vector3 rel=Quaternion.Inverse(bodyRot)*(actualEye-bodyPos);RawEyeRel=rel;
            Vector3 torso=TorsoEye(leanFb,leanLr,lookBack);
            trickWeight=Mathf.MoveTowards(trickWeight,trick?1:0,Mathf.Max(dt,0)*4);
            Vector3 target=torso;
            if(headMode!=HeadModeChassis){Vector3 d=rel-torso;d*=Mathf.Clamp01((6-d.magnitude)*.5f);d.x=Soft(d.x,headLimit.x,headLimit.x);d.y=Soft(d.y,headLimit.y,headLimitUp);d.z=Soft(d.z,headLimit.z,headLimit.z);target+=d*(1-trickWeight)*headBob;}
            SpringEye(target,dt);Vector3 eye=bodyPos+bodyRot*eyeRel;
            Quaternion frame=SmoothFrame(ViewFrame(bodyRot,headMode==HeadModeChassis,false),dt);
            LeanDeg=Lean(frame,bodyRot);float frameDt=FrameTime(out bool newFrame);
            float ca=Mathf.Clamp(CornerAnticipation(bodyPos,frame*Vector3.forward,speed),-60,60);CornerTarget=ca;
            if(!yawPrimed){caYaw=ca;caYawVel=0;}
            else if(dt>0){float w=1/.12f,k=Mathf.Exp(-w*dt),e=caYaw-ca,c=caYawVel+w*e;caYaw=ca+(e+c*dt)*k;caYawVel=(caYawVel-w*dt*c)*k;}
            float targetYaw=Mathf.Clamp(StickYaw()+caYaw,-60,60);
            yaw=!yawPrimed?targetYaw:Mathf.MoveTowards(yaw,targetYaw,600*Mathf.Max(frameDt,dt));yawPrimed=true;
            if(freeLook&&newFrame)MouseLook(frameDt);else if(!freeLook)mouseYaw=mousePitch=0;
            float look=DynamicPitch(leanFb),roll=LeanDeg*tilt;
            Quaternion q=frame*Quaternion.AngleAxis(-roll,Vector3.forward);
            q=lookBack?q*Quaternion.AngleAxis(180,Vector3.up):q*Quaternion.AngleAxis(yaw+mouseYaw+CommandYaw,Vector3.up)*Quaternion.AngleAxis(look+mousePitch,Vector3.right);
            PitchDeg=lookBack?0:look+mousePitch;RollDeg=roll;
            eye+=frame*offset;Vector3 sh=Vector3.zero;
            if(headShift){Vector3 a=accBox.Out;sh=new Vector3(Mathf.Clamp(-.015f*a.x*shiftLat,-.045f,.045f),Mathf.Clamp(-.015f*a.y*shiftVert,-.045f,.045f),Mathf.Clamp(-.025f*a.z*shiftLong,-.075f,.075f))/Ft;eye+=q*sh;}
            HeadShift=sh;Log(dt,bodyRot,q,eye,bodyPos,leanFb);return Publish(head,eye,q);
        }
        float FilterLeanForward(float input,float dt)
        {
            RawLeanForward=Mathf.Clamp(input,-1,1);
            if(!leanPrimed){filteredLeanForward=RawLeanForward;leanPrimed=true;}
            else if(dt>0)filteredLeanForward=Mathf.MoveTowards(filteredLeanForward,RawLeanForward,SourceLeanRate*dt);
            return filteredLeanForward;
        }
        bool Reject(string reason)
        {
            Deactivate();
            float now = Time.realtimeSinceStartup;
            if (fallbackSince < 0f || now - lastRejectTime > 0.5f || reason != FallbackReason) fallbackSince = now;
            lastRejectTime = now; FallbackReason = reason;
            bool crash = reason == "wrecked" || reason == "rider-inactive";
            if (fallbackLogged != reason && now - fallbackSince >= (crash ? WreckLogAfter : FallbackLogAfter) && fallbackLogs < MaxFallbackLogs)
            {
                fallbackLogs++; fallbackLogged = reason;
                Debug.Log($"[FPV] fallback: {reason} - the game's camera shows instead of DYNAMIC FPV ({now - fallbackSince:0.0} s so far; head mode {headMode}, machine {isMachine})");
            }
            return false;
        }

        // ---- the fallback (the game's camera instead of this one), for the player's log and the options page (w-fpvfix's, kept by w-fpvrevert)
        // A fallback that lasts (FallbackLogAfter; a wreck: WreckLogAfter) is logged once with its reason ("[FPV] fallback: ..."), and its end once
        // ("[FPV] active again ..."), at most MaxFallbackLogs times a session. FallbackSeconds: how long the current one has lasted (0: active).
        const float FallbackLogAfter = 1.5f, WreckLogAfter = 12f; const int MaxFallbackLogs = 40;
        static int fallbackLogs;
        float fallbackSince = -1f, lastRejectTime = -10f; string fallbackLogged;
        public float FallbackSeconds => !Active && fallbackSince >= 0f && Time.realtimeSinceStartup - lastRejectTime < 0.5f ? Time.realtimeSinceStartup - fallbackSince : 0f;
        /// Why the game's camera shows, in words (the options page): "" while this camera is on screen or it has not been asked to.
        public string FallbackText => FallbackSeconds <= 0f ? "" : DescribeFallback(FallbackReason);
        public static string DescribeFallback(string reason)
        {
            switch (reason)
            {
                case "wrecked": case "rider-inactive": return "the rider is off the bike (a crash): the game's camera until he is back on";
                case "machine-seat-not-posed": return "this vehicle's driver is not posed";
                case "authored-pose-missing": case "eye-bones-missing": case "eye-bone-transform-missing": return "the rider model has no head / eye bones";
                case "rider-missing": return "no rider model";
                default: return reason;
            }
        }
        void FallbackEnded()
        {
            if (fallbackLogged != null && fallbackLogs < MaxFallbackLogs) { fallbackLogs++; Debug.Log($"[FPV] active again after {Time.realtimeSinceStartup - fallbackSince:0.0} s of {fallbackLogged}"); }
            fallbackLogged = null; fallbackSince = -1f;
        }

        // ---- w-fpvrevert: VIEW SMOOTHING. The view frame's up (its pitch and roll; the heading stays the travel frame's) on a critically damped spring
        // of time constant viewSmoothing (exact form: the same at any frame rate); 0 = the frame as it is (the old camera).
        Vector3 viewUp, viewUpVel; bool viewPrimed;
        Quaternion SmoothFrame(Quaternion frame,float dt)
        {
            Vector3 up=frame*Vector3.up;
            if(viewSmoothing<=1e-4f||!viewPrimed){viewUp=up;viewUpVel=Vector3.zero;viewPrimed=true;return frame;}
            if(dt>0){float w=1/viewSmoothing,k=Mathf.Exp(-w*dt);Vector3 e=viewUp-up,c=viewUpVel+w*e;viewUp=up+(e+c*dt)*k;viewUpVel=(viewUpVel-w*dt*c)*k;}
            Vector3 u=viewUp.sqrMagnitude>1e-8f?viewUp.normalized:up,f=Vector3.ProjectOnPlane(frame*Vector3.forward,u);
            if(f.sqrMagnitude<1e-8f||Vector3.Dot(u,up)<.5f){viewUp=up;viewUpVel=Vector3.zero;return frame;}
            return Quaternion.LookRotation(f.normalized,u);
        }

        // REFLEX_FPV_CAMLOG=<csv> (w-fpvrevert): the internals per frame (tests/fpv_camera): the filter dt, the view's pitch / roll / yaw relative to the
        // bike, the eye in the bike's frame, the raw head eye, the lean input, the lean / pitch / roll the camera computed, the head shift, the trick
        // weight, the clearance to the hands' line.
        static readonly string camLogPath = Environment.GetEnvironmentVariable("REFLEX_FPV_CAMLOG");
        StreamWriter camLog;
        void Log(float dt,Quaternion bodyRot,Quaternion view,Vector3 eye,Vector3 bodyPos,float leanFb)
        {
            if (string.IsNullOrEmpty(camLogPath) || ownSettings) return;
            if (camLog == null)
            {
                try { camLog = new StreamWriter(camLogPath, false, new System.Text.UTF8Encoding(false), 1 << 16); } catch (Exception) { return; }
                camLog.WriteLine("frame,t,dt,viewPitch,viewRoll,viewYaw,ex,ey,ez,rawx,rawy,rawz,leanFb,leanDeg,pitchDeg,rollDeg,shx,shy,shz,trick");
            }
            Quaternion rel=Quaternion.Inverse(bodyRot)*view;Vector3 eu=rel.eulerAngles,e=Quaternion.Inverse(bodyRot)*(eye-bodyPos);
            float A(float a)=>a>180?a-360:a;
            string F(float v) => v.ToString("0.#####", CultureInfo.InvariantCulture);
            camLog.WriteLine(string.Join(",", Time.frameCount.ToString(CultureInfo.InvariantCulture), F((float)clock), F(dt), F(A(eu.x)), F(A(eu.z)), F(A(eu.y)), F(e.x), F(e.y), F(e.z),
                F(RawEyeRel.x), F(RawEyeRel.y), F(RawEyeRel.z), F(leanFb), F(LeanDeg), F(PitchDeg), F(RollDeg), F(HeadShift.x), F(HeadShift.y), F(HeadShift.z), F(trickWeight)));
        }
        void SpringEye(Vector3 target,float dt)
        {
            if(!eyePrimed||headSmoothing<=0){eyeRel=target;eyeVel=Vector3.zero;}
            else if(dt>0){float w=1/headSmoothing,k=Mathf.Exp(-w*dt);Vector3 e=eyeRel-target,c=eyeVel+w*e;eyeRel=target+(e+c*dt)*k;eyeVel=(eyeVel-w*dt*c)*k;}
            eyePrimed=true;EyeRel=eyeRel;
        }
        static float Lean(Quaternion frame,Quaternion body){Vector3 u=Quaternion.Inverse(frame)*(body*Vector3.up);return Mathf.Atan2(u.x,u.y)*Mathf.Rad2Deg;}
        Quaternion ViewFrame(Quaternion body,bool chassis,bool machine)
        {
            Vector3 f,u,r;
            if(chassis){f=body*Vector3.forward;r=Vector3.Cross(Vector3.up,f);if(r.sqrMagnitude<1e-8f)r=Vector3.Cross(body*Vector3.up,f);r.Normalize();u=Vector3.Cross(f,r).normalized;if(!machine&&Vector3.Dot(body*Vector3.up,u)<-.5f){r=Vector3.Cross(Vector3.down,f).normalized;u=Vector3.Cross(f,r).normalized;}}
            else {TravelFrame(body,out f,out u,out r,machine);if(Vector3.Dot(body*Vector3.up,u)<-.5f){u=-u;r=-r;}}
            return Quaternion.LookRotation(Vector3.Cross(r,u),u);
        }
        void TravelFrame(Quaternion bodyRot,out Vector3 fwd,out Vector3 up,out Vector3 right,bool machine=false)
        {
            up=upBox.Out.sqrMagnitude>1e-6f?upBox.Out.normalized:Vector3.up;
            Vector3 fv=velBox.Out,vp=fv-up*Vector3.Dot(fv,up);float vm=vp.magnitude*Ft;fwd=bodyRot*Vector3.forward;
            bool backwards=machine&&Vector3.Dot(vp,fwd)<0;
            if(!backwards&&vm>=5)fwd=vp.normalized;else if(!backwards&&vm>1){float k=(vm-1)*.25f;fwd=Vector3.Normalize(fwd*(1-k)+vp.normalized*k);}
            right=Vector3.Cross(up,fwd);right.y=0;if(right.sqrMagnitude<1e-8f)right=Vector3.Cross(bodyRot*Vector3.up,fwd);right.Normalize();up=Vector3.Cross(fwd,right).normalized;fwd=Vector3.Cross(right,up);
            TravelForward=fwd;TravelSpeedMetres=vm;TravelBlend=backwards?0:Mathf.Clamp01((vm-1)*.25f);
        }
        float DynamicPitch(float leanFb)=>pitch*20+15+(leanPitch&&leanFb>0?leanFb*10:0);
        float FrameTime(out bool newFrame){newFrame=lastFrame!=Time.frameCount;lastFrame=Time.frameCount;return newFrame?Time.unscaledDeltaTime:0;}
        float StickYaw() { var pad = LocalGamepadRouting.ActiveLocalPad; return freeLook && !rightStickRiding && pad != null ? HeadingCurve(pad.rightStick.x.ReadValue()) * 60 : 0; }
        static float HeadingCurve(float x){const float dz=.15f;if(Mathf.Abs(x)<dz)return 0;float a=(Mathf.Abs(x)-dz)/(1-dz);return Mathf.Sign(x)*a*a;}
        static float? commandYaw;
        static float CommandYaw {get {if(!commandYaw.HasValue){var a=Environment.GetCommandLineArgs();int i=Array.IndexOf(a,"-reflexLookYaw");if(i<0)i=Array.IndexOf(a,"-mxaLookYaw");commandYaw=i>=0&&i+1<a.Length&&float.TryParse(a[i+1],System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out float y)?y:0;}return commandYaw.Value;}}
        bool Publish(Transform head,Vector3 eye,Quaternion q)
        {
            if(!Finite(eye)||!Finite(q*Vector3.forward)||!Finite(q*Vector3.up))return Reject("view-nonfinite");
            EyeInHead=head.InverseTransformPoint(eye);cam.transform.SetPositionAndRotation(eye,q);cam.fieldOfView=VerticalFov(fov,cam.aspect);cam.nearClipPlane=nearPlane;
            if(!Active)SetCockpit(rider,true);Active=true;FallbackReason="";if(fallbackSince>=0f)FallbackEnded();return true;
        }
        bool ApplyTrue(Transform head,Vector3 actualEye,Quaternion gaze,Vector3 bodyPos,Quaternion bodyRot,Vector3 vel,float leanFb,float dt,bool lookBack)
        {
            Quaternion ib=Quaternion.Inverse(bodyRot);Vector3 rel=ib*(actualEye+gaze*offset-bodyPos);Quaternion rr=ib*gaze;RawEyeRel=rel;
            float tau=trueSmoothing*.25f;
            if(!truePrimed||tau<=1e-4f){trueEye=rel;trueVel=Vector3.zero;trueRot=rr;}
            else if(dt>0){float w=1/tau,k=Mathf.Exp(-w*dt);Vector3 e=trueEye-rel,c=trueVel+w*e;trueEye=rel+(e+c*dt)*k;trueVel=(trueVel-w*dt*c)*k;trueRot=Quaternion.Slerp(trueRot,rr,1-Mathf.Exp(-dt/(tau*.6f)));}
            truePrimed=true;eyePrimed=false;EyeRel=trueEye;trickWeight=0;
            Quaternion q=TrueRoll(TrueTravel(bodyRot*trueRot,bodyRot,trueYawSmoothing,truePitchSmoothing,DynamicPitch(leanFb)),trueRoll);
            LeanDeg=Lean(q,bodyRot);float frameDt=FrameTime(out bool newFrame);yaw=StickYaw();
            if(freeLook&&newFrame)MouseLook(frameDt);else if(!freeLook)mouseYaw=mousePitch=0;
            q=lookBack?q*Quaternion.AngleAxis(180,Vector3.up):q*Quaternion.AngleAxis(yaw+mouseYaw+CommandYaw,Vector3.up)*Quaternion.AngleAxis(mousePitch,Vector3.right);
            Vector3 eul=(ib*q).eulerAngles;PitchDeg=eul.x>180?eul.x-360:eul.x;RollDeg=eul.z>180?eul.z-360:eul.z;HeadShift=Vector3.zero;CornerTarget=0;
            Log(dt,bodyRot,q,bodyPos+bodyRot*trueEye,bodyPos,leanFb);
            return Publish(head,bodyPos+bodyRot*trueEye,q);
        }
        bool ApplyMachine(Transform head,Vector3 actualEye,Vector3 bodyPos,Quaternion bodyRot,float dt,bool lookBack)
        {
            Vector3 rel=Quaternion.Inverse(bodyRot)*(actualEye-bodyPos);RawEyeRel=rel;
            if(rel.sqrMagnitude>225)return Reject("machine-seat-not-posed");
            if(!seatSet||!eyePrimed||(rel-seatEye).sqrMagnitude>36){seatEye=rel;seatSet=true;eyePrimed=false;}
            Vector3 d=rel-seatEye;d.x=Soft(d.x,.35f,.35f);d.y=Soft(d.y,.25f,.35f);d.z=Soft(d.z,.35f,.35f);SpringEye(seatEye+d,dt);
            Quaternion frame=ViewFrame(bodyRot,headMode==HeadModeChassis,true);LeanDeg=Lean(frame,bodyRot);float look=pitch*10+5,roll=LeanDeg*tilt;
            Quaternion q=frame*Quaternion.AngleAxis(-roll,Vector3.forward);q=lookBack?q*Quaternion.AngleAxis(180,Vector3.up):q*Quaternion.AngleAxis(CommandYaw,Vector3.up)*Quaternion.AngleAxis(look,Vector3.right);
            PitchDeg=lookBack?0:look;RollDeg=roll;HeadShift=Vector3.zero;CornerTarget=0;trickWeight=0;
            return Publish(head,bodyPos+bodyRot*eyeRel+frame*offset,q);
        }
        Vector3 TorsoEye(float leanFb,float leanLr,bool lookBack)
        {
            float a=lookBack?10:leanFb>0?leanFb*20+20:-leanFb*10+20,th=(90-a)*Mathf.Deg2Rad;
            return TorsoPivot+new Vector3(.2f/Ft*leanLr,Mathf.Sin(th)*TorsoLen,.2f/Ft*leanFb+Mathf.Cos(th)*TorsoLen);
        }
        static float Soft(float x,float lo,float hi){float l=x<0?lo:hi;return l>0?l*(float)Math.Tanh(x/l):0;}
        void MouseLook(float dt)
        {
            var m=Mouse.current;Vector2 d=m!=null?m.delta.ReadValue():Vector2.zero;
            if(d.sqrMagnitude>.01f){mouseYaw=Mathf.Clamp(mouseYaw+d.x*.15f,-120,120);mousePitch=Mathf.Clamp(mousePitch-d.y*.15f,-60,60);mouseIdle=0;return;}
            mouseIdle+=dt;if(mouseIdle>1.5f){float k=1-Mathf.Exp(-dt/.25f);mouseYaw=Mathf.Lerp(mouseYaw,0,k);mousePitch=Mathf.Lerp(mousePitch,0,k);}
        }
        Quaternion TrueRoll(Quaternion q, float amount)
        {
            if (amount >= 0.999f) return q;
            Vector3 f = q * Vector3.forward;
            if (!LevelUp(f, out Vector3 levelUp)) return q;
            float keep = Mathf.Lerp(1f, Mathf.Clamp01(amount), Upright(q, levelUp));
            return Quaternion.Slerp(Quaternion.LookRotation(f, levelUp), q, keep);
        }

        /// The level up for a gaze forward f (DYNAMIC's: the smoothed ground normal, right kept horizontal); false with
        /// the gaze straight up / down.
        bool LevelUp(Vector3 f, out Vector3 levelUp)
        {
            Vector3 upRef = upBox.Out.sqrMagnitude > 1e-6f ? upBox.Out.normalized : Vector3.up;
            Vector3 right = Vector3.Cross(upRef, f);
            right.y = 0f;
            levelUp = upRef;
            if (right.sqrMagnitude < 1e-6f) return false;
            levelUp = Vector3.Cross(f, right.normalized).normalized;
            return true;
        }

        /// 1 while the rider rides upright, fading to 0 as he stops being: the head's up within ~72 deg of the level up
        /// (a hard corner's lean too; gone past 96 deg), the gaze more than ~18 deg off vertical.
        float Upright(Quaternion q, Vector3 levelUp)
        {
            Vector3 f = q * Vector3.forward, u = q * Vector3.up;
            Vector3 upRef = upBox.Out.sqrMagnitude > 1e-6f ? upBox.Out.normalized : Vector3.up;
            return Mathf.Clamp01((Vector3.Dot(u, levelUp) + 0.1f) / 0.4f) * Mathf.Clamp01((0.95f - Mathf.Abs(Vector3.Dot(f, upRef))) / 0.25f);
        }

        /// TRUE: the head's gaze and the view (before free look / look back) in DYNAMIC's travel frame, (yaw + = right,
        /// elevation + = up) deg (diagnostics, -mxaOnbPitch).
        public Vector2 TrueHeadDir { get; private set; }
        public Vector2 TrueViewDir { get; private set; }

        /// TRUE's YAW / PITCH SMOOTHING: the gaze `q`'s yaw and pitch moved towards DYNAMIC's view (the smoothed direction
        /// of travel, DYNAMIC's look-down) by `yawK` / `pitchK` (0 = the head's own, 1 = DYNAMIC's), its roll over the
        /// level horizon kept (HEAD ROLL acts on it after). Faded back to the head's own as HEAD ROLL is (flips, crashes)
        /// and as the head turns more than 90 deg from the direction of travel.
        Quaternion TrueTravel(Quaternion q, Quaternion bodyRot, float yawK, float pitchK, float lookDown)
        {
            Vector3 f = q * Vector3.forward, u = q * Vector3.up;
            TravelFrame(bodyRot, out Vector3 tf, out Vector3 tu, out _);
            Quaternion frame = Quaternion.LookRotation(tf, tu);
            Vector3 fl = Quaternion.Inverse(frame) * f;
            float yaw = Mathf.Atan2(fl.x, fl.z) * Mathf.Rad2Deg;
            float elev = Mathf.Asin(Mathf.Clamp(fl.y, -1f, 1f)) * Mathf.Rad2Deg;
            TrueHeadDir = TrueViewDir = new Vector2(yaw, elev);
            if (yawK <= 1e-4f && pitchK <= 1e-4f || !LevelUp(f, out Vector3 levelUp)) return q;
            float w = Upright(q, levelUp) * Mathf.Clamp01((90f - Mathf.Abs(yaw)) / 30f);
            yawK = Mathf.Clamp01(yawK) * w;
            pitchK = Mathf.Clamp01(pitchK) * w;
            if (yawK <= 1e-4f && pitchK <= 1e-4f) return q;
            yaw *= 1f - yawK;
            elev = Mathf.Lerp(elev, -lookDown, pitchK);
            TrueViewDir = new Vector2(yaw, elev);
            float cy = Mathf.Cos(yaw * Mathf.Deg2Rad), sy = Mathf.Sin(yaw * Mathf.Deg2Rad);
            float ce = Mathf.Cos(elev * Mathf.Deg2Rad), se = Mathf.Sin(elev * Mathf.Deg2Rad);
            Vector3 f2 = frame * new Vector3(sy * ce, se, cy * ce);
            if (!LevelUp(f2, out Vector3 levelUp2)) return q;
            float roll = Vector3.SignedAngle(levelUp, Vector3.ProjectOnPlane(u, f), f);
            return Quaternion.AngleAxis(roll, f2) * Quaternion.LookRotation(f2, levelUp2);
        }


        Vector3 Pt(int i)=>new Vector3(raceLine[3*i],raceLine[3*i+1],raceLine[3*i+2]);
        // The native API returns a linked route slice ahead, which can stop at its buffer cap.
        // Close it only with authoritative topology metadata; never add a chord across a truncated slice.
        int Next(int i) => i+1<raceLinePoints ? i+1 : raceLineClosed ? 0 : -1;
        Vector3 LineDir(int i)
        {
            int n=i>=0?Next(i):-1;
            if(n<0)return Vector3.zero;
            Vector3 d=Pt(n)-Pt(i);d.y=0;
            return d.sqrMagnitude>1e-6f?d.normalized:Vector3.zero;
        }
        Vector3 Tangent(int i,float u)
        {
            Vector3 d=LineDir(i);
            if(u<.5f)
            {
                int previous=i>0?i-1:raceLineClosed?raceLinePoints-1:-1;
                Vector3 before=previous>=0?LineDir(previous):d;
                d=Vector3.Lerp(before+d,d*2,u*2);
            }
            else
            {
                Vector3 after=LineDir(Next(i));if(after.sqrMagnitude<1e-6f)after=d;
                d=Vector3.Lerp(d*2,d+after,u*2-1);
            }
            return d.sqrMagnitude>1e-8f?d.normalized:LineDir(i);
        }
        float CornerAnticipation(Vector3 pos,Vector3 fwd,float speed)
        {
            if(cornerAnticipation==2)return Mathf.Clamp(LeanDeg/50,-1,1)*leanHeadingScale;
            if(cornerAnticipation!=1||raceLinePoints<2)return 0;
            int best=-1;float bd=float.MaxValue,u=0;
            for(int i=0;i<raceLinePoints;i++)
            {
                int n=Next(i);if(n<0)continue;
                Vector3 a=Pt(i),ab=Pt(n)-a,ap=pos-a;ab.y=ap.y=0;
                float x=ab.sqrMagnitude>1e-6f?Mathf.Clamp01(Vector3.Dot(ap,ab)/ab.sqrMagnitude):0;
                float distance=(ap-ab*x).sqrMagnitude;
                if(distance<bd){bd=distance;best=i;u=x;}
            }
            if(best<0||bd>22500)return 0;
            Vector3 t0=Tangent(best,u);int k=best;
            float left=Mathf.Max(speed*Ft*1.2f,15)/Ft;
            for(int guard=0;guard<raceLinePoints;guard++)
            {
                int n=Next(k);if(n<0)break;
                Vector3 seg=Pt(n)-Pt(k);seg.y=0;
                float len=seg.magnitude,rest=(1-u)*len;
                if(left<=rest||len<1e-4f&&left<=0){u+=len>1e-4f?left/len:0;break;}
                if(Next(n)<0){u=1;break;}
                left-=rest;k=n;u=0;
            }
            Vector3 target=Vector3.Lerp(Pt(k),Pt(Next(k)),u),t1=Tangent(k,u),to=target-pos,f=fwd;to.y=f.y=0;
            if(to.sqrMagnitude<1||f.sqrMagnitude<1e-6f)return 0;
            float turn=t0.sqrMagnitude>1e-4f&&t1.sqrMagnitude>1e-4f?Vector3.SignedAngle(t0,t1,Vector3.up):0;
            float ang=Vector3.SignedAngle(f,to,Vector3.up);
            return ang*Mathf.SmoothStep(0,1,Mathf.InverseLerp(1,8,turn*Mathf.Sign(ang)))*cornerAnticipationScale;
        }
        float UpdateFilters(Vector3 pos,Quaternion rot,Vector3 front,Vector3 rear,Vector3 vel,Terrain terrain)
        {
            bool timed=!double.IsNaN(renderClock)&&!double.IsInfinity(renderClock);
            float elapsed=timed?(float)(renderClock-clock):pendingDt;pendingDt=0;
            bool teleport=primed&&(pos-lastPos).sqrMagnitude>900;
            if(!primed||teleport||resetPending||timed&&renderClock<clock-.5)
            {
                upSrc=Vector3.up;velSrc=vel;accSrc=Vector3.zero;upBox.Fill(upSrc);velBox.Fill(velSrc);accBox.Fill(accSrc);
                prevVel=vel;haveVel=false;prevElapsed=stepAcc=0;lastPos=pos;primed=true;eyePrimed=yawPrimed=truePrimed=seatSet=leanPrimed=false;trickWeight=0;resetPending=false;clock=timed?renderClock:0;return 0;
            }
            if(elapsed<=0)return 0;clock=timed?renderClock:clock+elapsed;lastPos=pos;
            Vector3 up=GroundUp(front,rear,terrain),acc=Vector3.zero;
            if(haveVel){float span=.5f*(elapsed+prevElapsed);acc=Quaternion.Inverse(rot)*((vel-prevVel)/Mathf.Max(span,1e-4f))/32.174f;}
            haveVel=true;prevVel=vel;prevElapsed=elapsed;float acc0=stepAcc;stepAcc+=elapsed;int n=(int)((stepAcc+1e-6f)/Step);stepAcc=Mathf.Max(0,stepAcc-n*Step);
            upBox.Push(upSrc,up,n,acc0,elapsed,stepAcc/Step);velBox.Push(velSrc,vel,n,acc0,elapsed,stepAcc/Step);accBox.Push(accSrc,acc,n,acc0,elapsed,stepAcc/Step);
            upSrc=up;velSrc=vel;accSrc=acc;return elapsed;
        }
        Vector3 GroundUp(Vector3 front,Vector3 rear,Terrain terrain)
        {
            if(!terrain)return Vector3.up;Vector3 sum=Vector3.zero;
            if(haveContacts?frontGrounded:NearGround(front,terrain))sum+=NormalAt(front,terrain);
            if(haveContacts?rearGrounded:NearGround(rear,terrain))sum+=NormalAt(rear,terrain);
            return sum.sqrMagnitude>1e-6f&&sum.y>0?sum.normalized:Vector3.up;
        }
        static bool NearGround(Vector3 hub,Terrain terrain)=>hub.y-(terrain.SampleHeight(hub)+terrain.transform.position.y)<=1.6f;
        static Vector3 NormalAt(Vector3 p,Terrain terrain)
        {
            // Same 1.5ft finite-difference stencil as Open Unleashed's live terrain query.
            const float h=1.5f;var td=terrain.terrainData;Vector3 o=terrain.transform.position;
            if(p.x-h<o.x||p.x+h>o.x+td.size.x||p.z-h<o.z||p.z+h>o.z+td.size.z)return Vector3.up;
            float xl=terrain.SampleHeight(p+Vector3.left*h),xr=terrain.SampleHeight(p+Vector3.right*h),zb=terrain.SampleHeight(p+Vector3.back*h),zf=terrain.SampleHeight(p+Vector3.forward*h);
            return terrain.transform.TransformDirection(new Vector3(xl-xr,2*h,zb-zf).normalized).normalized;
        }
        sealed class Box
        {
            readonly Vector3[] buf; int head;
            public Vector3 Out { get; private set; }
            public Box(float window) { buf = new Vector3[Mathf.Max(1, Mathf.RoundToInt(window / Step))]; }
            public void Fill(Vector3 v) { for (int i = 0; i < buf.Length; i++) buf[i] = v; head = 0; Out = v; }
            public void Push(Vector3 a, Vector3 b, int n, float acc0, float elapsed, float frac)
            {
                int first = Mathf.Max(1, n - buf.Length + 1);
                for (int j = first; j <= n; j++)
                {
                    float t = Mathf.Clamp01((j * Step - acc0) / elapsed);
                    buf[head] = Vector3.LerpUnclamped(a, b, t);
                    head = (head + 1) % buf.Length;
                }
                Vector3 sum = Vector3.zero;
                for (int i = 0; i < buf.Length; i++) sum += buf[i];
                sum += frac * (b - buf[head]);
                Out = sum / buf.Length;
            }
        }

        // ------------------------------------------------------------------------- the cockpit rider
        // MX Bikes' onboard rider: the forearms, gloves and legs; the rest hidden (triangles whose
        // dominant bone is a hidden node are dropped), the whole rider still casting the shadow.
        static readonly string[] HiddenNodes =
        {
            "MXRider_Root", "MXRider_SpineA", "MXRider_SpineB", "MXRider_R_Flap", "MXRider_M_Flap", "MXRider_L_Flap",
            "MXRider_Chest", "NeckBrace", "MXRider_RCollar", "MXRider_RCollar_Flap", "MXRider_LCollar", "MXRider_LCollar_Flap",
            "MXRider_RChest_Flap", "MXRider_LChest_Flap", "ChestProtector", "MXRider_Neck", "MXRider_Head",
            "MXRider_REye", "MXRider_LEye", "MXRider_Blink", "Helmet", "Goggles",
            // the upper arms too (w-fpvrewrite's e6cba105, kept by w-fpvrevert): no sleeve at the edges of the view; the forearms and gloves stay
            "MXRider_RArmA", "MXRider_LArmA", "MXRider_RArm_FlapA", "MXRider_RArm_FlapB", "MXRider_LArm_FlapA", "MXRider_LArm_FlapB",
        };

        sealed class Cockpit { public SkinnedMeshRenderer smr, shadow; public Mesh full, cockpit; }
        static readonly System.Collections.Generic.Dictionary<OriginalRider, Cockpit[]> cockpits = new System.Collections.Generic.Dictionary<OriginalRider, Cockpit[]>();

        static void ReleaseCockpit(OriginalRider r)
        {
            if(ReferenceEquals(r,null)||!cockpits.TryGetValue(r,out var parts))return;
            foreach(var c in parts){if(c.smr){c.smr.sharedMesh=c.full;c.smr.shadowCastingMode=UnityEngine.Rendering.ShadowCastingMode.On;}if(c.shadow)UnityEngine.Object.Destroy(c.shadow.gameObject);if(c.cockpit)UnityEngine.Object.Destroy(c.cockpit);}
            cockpits.Remove(r);
        }

        /// The cockpit rider (no head and torso; their shadows kept) on or off: the OPEN REFLEX replay's onboard camera.
        public static void SetCockpitView(OriginalRider r, bool on) { if (r) SetCockpit(r, on); }

        static void SetCockpit(OriginalRider r, bool on)
        {
            if (!cockpits.TryGetValue(r, out var parts)) cockpits[r] = parts = BuildCockpit(r);
            foreach (var c in parts)
            {
                if (!c.smr) continue;
                c.smr.sharedMesh = on && c.cockpit ? c.cockpit : c.full;
                c.smr.shadowCastingMode = on ? UnityEngine.Rendering.ShadowCastingMode.Off : UnityEngine.Rendering.ShadowCastingMode.On;
                if (c.shadow) c.shadow.enabled = on && c.smr.enabled;
            }
            if (r.rigidMeshes != null)
                foreach (var m in r.rigidMeshes)
                    if (m) m.shadowCastingMode = on ? UnityEngine.Rendering.ShadowCastingMode.ShadowsOnly : UnityEngine.Rendering.ShadowCastingMode.On;
        }

        static Cockpit[] BuildCockpit(OriginalRider r)
        {
            var list = new System.Collections.Generic.List<Cockpit>();
            var smrs = r.skinnedMeshes != null && r.skinnedMeshes.Length > 0 ? r.skinnedMeshes : new[] { r.skinnedMesh };
            var hiddenNames = new System.Collections.Generic.HashSet<string>(HiddenNodes);
            foreach (var smr in smrs)
            {
                if (!smr || !smr.sharedMesh) continue;
                var c = new Cockpit { smr = smr, full = smr.sharedMesh };
                var mesh = smr.sharedMesh;
                if (mesh.isReadable)
                {
                    var bones = smr.bones; var bw = mesh.boneWeights;
                    var hidden = new bool[bones.Length];
                    for (int i = 0; i < bones.Length; i++) hidden[i] = bones[i] && hiddenNames.Contains(bones[i].name);
                    var cm = UnityEngine.Object.Instantiate(mesh); cm.name = mesh.name + " (cockpit)";
                    for (int s = 0; s < mesh.subMeshCount; s++)
                    {
                        var src = mesh.GetTriangles(s); var tri = new System.Collections.Generic.List<int>(src.Length);
                        for (int t = 0; t + 2 < src.Length; t += 3)
                        {
                            if (bw.Length > 0 && (hidden[bw[src[t]].boneIndex0] || hidden[bw[src[t + 1]].boneIndex0] || hidden[bw[src[t + 2]].boneIndex0])) continue;
                            tri.Add(src[t]); tri.Add(src[t + 1]); tri.Add(src[t + 2]);
                        }
                        cm.SetTriangles(tri, s, false);
                    }
                    c.cockpit = cm;
                    // shadow-only copy of the whole part, on the same bones
                    var go = new GameObject(smr.name + " (shadow)");
                    go.transform.SetParent(smr.transform.parent, false);
                    go.transform.localPosition = smr.transform.localPosition; go.transform.localRotation = smr.transform.localRotation; go.transform.localScale = smr.transform.localScale;
                    var sh = go.AddComponent<SkinnedMeshRenderer>();
                    sh.sharedMesh = mesh; sh.bones = smr.bones; sh.rootBone = smr.rootBone; sh.sharedMaterials = smr.sharedMaterials;
                    sh.updateWhenOffscreen = true; sh.localBounds = smr.localBounds;
                    sh.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.ShadowsOnly; sh.enabled = false;
                    c.shadow = sh;
                }
                else Debug.LogWarning("Dynamic FPV: rider mesh " + mesh.name + " is not readable; its head and torso stay visible onboard.");
                list.Add(c);
            }
            return list.ToArray();
        }
    }
}

using System;
namespace Reflex.Port.RxCore
{
    // Pure ownership policy used by the actual Unity camera adapter. No clock,
    // lens arithmetic, default values, or native replay state is synthesized.
    public struct ReplayCameraLens
    {
        public float FieldOfView, Near, Far;
        public ReplayCameraLens(float fov,float near,float far){FieldOfView=fov;Near=near;Far=far;}
    }
    public sealed class ReplayCameraLensLease
    {
        object owner; ReplayCameraLens saved,lastApplied;bool applied;
        public bool Active => owner != null;
        public void Acquire(object camera,ReplayCameraLens current)
        {
            if(camera==null)throw new ArgumentNullException(nameof(camera));
            if(owner!=null)
            {
                if(!ReferenceEquals(owner,camera))throw new InvalidOperationException("Replay camera changed during a lens lease.");
                return; // Keep the pre-replay lens, not a later director lens.
            }
            owner=camera;saved=current;
        }
        public void Applied(object camera,ReplayCameraLens actual)
        {
            if(owner==null||!ReferenceEquals(owner,camera))throw new InvalidOperationException("Replay lens write has no matching lease.");
            lastApplied=actual;applied=true;
        }
        static bool Equal(ReplayCameraLens a,ReplayCameraLens b)=>a.FieldOfView==b.FieldOfView&&a.Near==b.Near&&a.Far==b.Far;
        public bool Release(object camera,ReplayCameraLens current,out ReplayCameraLens restore)
        {
            restore=default;
            if(owner==null||!ReferenceEquals(owner,camera))return false;
            // A user/ordinary mode may already have replaced the lens before
            // this exit is observed. Relinquish without overwriting that edit.
            bool stillOurs=applied&&Equal(current,lastApplied);
            if(stillOurs)restore=saved;
            owner=null;saved=lastApplied=default;applied=false;return stillOurs;
        }
    }
}

using System;
namespace Reflex.Port.RxCore
{
    // Pure managed selection policy; never reads or acknowledges the native session.
    public static class ReplayPresentedFrame
    {
        public static ReplaySnapshot Select(ReplaySnapshot collected,ReplaySnapshot live,ulong nativeFrame,bool exactPose)
        {
            // Only command-only requests on the already-collected frame can supersede its snapshot.
            var view=exactPose&&live!=null&&live.Count>0&&live.View.frame==nativeFrame?live:collected;
            if(view==null)return null;
            if(view.View.frame!=nativeFrame)throw new InvalidOperationException("Presented replay and scene poses differ.");
            if(!exactPose&&(view.View.active!=0||view.Count>0||view.DrawFade))return null;
            return view;
        }
    }
}

using System;
namespace Reflex.Port.RxCore {
 public sealed partial class OpenReflexSimulation {
  // Reuses the ACTUAL worker/catalog arena lifetime. No new persistence lease
  // counter, native call or waiting; an active/closing session rejects edits.
  public static T WithoutActivePlayerSession<T>(Func<T> action) {
   if(action==null)throw new ArgumentNullException(nameof(action));
   lock(sessionOwnership){
    if(arenaFailure!=null)throw new InvalidOperationException(arenaFailure);
    if(activeSessions!=0)throw new InvalidOperationException("Close the current ride or wait for career loading before changing profile mode.");
    return action();
   }
  }
 }
}

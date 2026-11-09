using Reflex.Port.Content;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        string checkpointNormalStatus;
        bool checkpointNoticeShown;

        void RefreshCheckpointNotice()
        {
            if (_statusText == null) return;
            string error = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? CoreGarageProfile.CheckpointError : null;
            if (!string.IsNullOrEmpty(error))
            {
                _statusText.text = "LAST RIDE NOT SAVED · Garage saves separately.";
                _statusText.tooltip = error;
                _statusText.name = "player-checkpoint-notice";
                checkpointNoticeShown = true;
            }
            else if (checkpointNoticeShown)
            {
                _statusText.text = checkpointNormalStatus ?? "";
                _statusText.tooltip = "";
                _statusText.name = "";
                checkpointNoticeShown = false;
            }
        }
    }
}

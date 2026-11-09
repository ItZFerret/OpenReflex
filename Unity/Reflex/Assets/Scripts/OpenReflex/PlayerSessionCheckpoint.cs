using System;

namespace Reflex.Port.RxCore
{
    // All callbacks execute on the session owner thread. Implementations must not call Unity.
    public interface IPlayerSessionPersistence
    {
        byte[] Prepare(byte[] nativeDefaults);
        void Imported(byte[] validatedSnapshot);
        void Complete(byte[] settledSnapshot);
        void Failed(string error);
    }

    // Optional capability. Existing persistence implementers remain valid.
    // Success means durable publication; callers must not acknowledge a failed call.
    public interface IPlayerNonterminalPersistence
    {
        void Checkpoint(byte[] validatedSnapshot);
    }

    // Optional receipt visibility, separate from durable data publication.
    public interface IPlayerCheckpointReceiptFeedback
    {
        void Receipt(string message); // null clears only this session's own receipt notice
    }

    public interface IPlayerSnapshotBackend
    {
        byte[] Export();
        void Import(byte[] snapshot);
        void FinalizeRecords();
    }

    // Shared by the real worker and fault/order tests. It never saves a partial or faulted session.
    public sealed class PlayerSessionCheckpoint
    {
        readonly IPlayerSessionPersistence persistence;
        bool started, finished, checkpointing, durableCheckpoint;
        string receiptFailure;
        public PlayerSessionCheckpoint(IPlayerSessionPersistence persistence) { this.persistence = persistence; }
        public void Start(IPlayerSnapshotBackend native)
        {
            if (started || finished) throw new InvalidOperationException("Player session has already started or closed.");
            if (persistence != null)
            {
                native.Import(persistence.Prepare(native.Export()));
                persistence.Imported(native.Export());
            }
            started = true;
        }
        // Worker-only, between advances. Does not finalize records or close the session.
        // Request generation/id deduplication belongs to the owned host queue.
        public void Checkpoint(IPlayerSnapshotBackend native, bool healthy)
        {
            if (!started || finished || checkpointing) throw new InvalidOperationException("Player checkpoint is not available in this lifecycle state.");
            checkpointing = true;
            try
            {
                if (!healthy) throw new InvalidOperationException("An unhealthy session cannot be checkpointed.");
                var target = persistence as IPlayerNonterminalPersistence;
                if (target == null) throw new NotSupportedException("Persistence does not support nonterminal checkpoints.");
                target.Checkpoint(native.Export());
                durableCheckpoint = true;
            }
            catch (Exception e) { persistence?.Failed("Player progress was not saved: " + e.Message); throw; }
            finally { checkpointing = false; }
        }
        public void ReportCheckpointFailure(string message)
        {
            if (!started || finished) throw new InvalidOperationException("Player checkpoint is closed.");
            receiptFailure = message;
            var feedback = persistence as IPlayerCheckpointReceiptFeedback;
            if (feedback != null) feedback.Receipt(message);
            else if (message != null) persistence?.Failed(message);
        }
        // Only after the current native completion reports both durable receipt IDs.
        // Existing checkpoints already published the final data; do not finalize or save again.
        public void FinishAlreadyCheckpointed()
        {
            if (finished) return;
            if (!started || checkpointing || !durableCheckpoint || receiptFailure != null)
                throw new InvalidOperationException("Final career checkpoint has not been confirmed.");
            finished = true;
        }
        public void Finish(IPlayerSnapshotBackend native, bool healthy) => Finish(native, healthy, null);
        // betweenRecordsAndExport (the tutorials' end): the session's own last step, run after the records' settlement and before the export
        // (the profile saved is the one after it); it runs whatever the outcome, also when nothing is saved.
        public void Finish(IPlayerSnapshotBackend native, bool healthy, Action betweenRecordsAndExport)
        {
            if (betweenRecordsAndExport != null)
            {
                if (checkpointing || finished || persistence == null || !started || !healthy)
                {
                    try { Finish(native, healthy, null); }
                    finally { betweenRecordsAndExport(); }
                    return;
                }
                finished = true;
                try { native.FinalizeRecords(); }
                catch (Exception e) { persistence.Failed("Player progress was not saved: " + e.Message); betweenRecordsAndExport(); throw; }
                betweenRecordsAndExport();
                try { persistence.Complete(native.Export()); }
                catch (Exception e) { persistence.Failed("Player progress was not saved: " + e.Message); throw; }
                return;
            }
            if (checkpointing) throw new InvalidOperationException("A checkpoint is in progress.");
            if (finished) return;
            finished = true;
            if (persistence == null) return;
            if (!started || !healthy)
            {
                persistence.Failed(durableCheckpoint
                    ? (receiptFailure ?? "An earlier player checkpoint was saved. Changes after that checkpoint were not saved because the session closed without final settlement.")
                    : "The interrupted session was not saved. Your previous profile is intact.");
                return;
            }
            try
            {
                native.FinalizeRecords();
                persistence.Complete(native.Export());
            }
            catch (Exception e) { persistence.Failed("Player progress was not saved: " + e.Message); throw; }
        }
    }

    internal sealed class NativePlayerSnapshotBackend : IPlayerSnapshotBackend
    {
        readonly IntPtr session;
        public NativePlayerSnapshotBackend(IntPtr session) { this.session = session; }
        public byte[] Export() => RxApiNative.ExportPlayer(session);
        public void Import(byte[] snapshot)
        {
            if (RxApiNative.ImportPlayer(session, snapshot) != 0) throw new InvalidOperationException(RxApiNative.rx_session_last_error(session));
        }
        public void FinalizeRecords()
        {
            if (RxApiNative.rx_session_finalize_records(session) != 0) throw new InvalidOperationException(RxApiNative.rx_session_last_error(session));
        }
    }
}

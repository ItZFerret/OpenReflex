using System;
using System.Collections.Generic;
using System.IO;

namespace Reflex.Port.Content
{
    public static partial class CoreGarageProfile
    {
        // The profile's PlayerEvent table (EventId string, Status int): MXTableHelper::GetPlayerEventStatus / SetPlayerEventStatus
        // (004eaaf0 / 004eabd0: a missing row reads 0, a set writes the row of the name or adds it). The front end's one-time pages keep
        // their "seen" flags there as the original's Lua does: "CareerWelcome" (MainMenu:CareerMode / CareerWelcome:OnAcceptKeypress),
        // "CareerCompleted" (RaceAndRide:Present), "MotocardWelcome" is this port's (the first sign-in), "Rule_<RulesType>"
        // (RulesPopup:OnToggleKeypress, the "do not show again" of a rules page).

        /// The status of an event name (0 when there is no row or no profile).
        public static int EventStatus(string name)
        {
            if (string.IsNullOrEmpty(name)) return 0;
            lock (gate)
            {
                if (!Open()) return 0;
                var t = current.T("PlayerEvent");
                for (int r = 0; r < t.rows; r++)
                    if (string.Equals(t.Str("EventId", r), name, StringComparison.Ordinal)) return t.Int("Status", r, 0);
                return 0;
            }
        }

        /// SetPlayerEventStatus: the row of the name is changed, or added; saved with the profile. False (with Error) when it could not be saved.
        public static bool SetEventStatus(string name, int value)
        {
            if (string.IsNullOrEmpty(name) || name.Length > 63) { Error = "Invalid event name."; return false; }
            return Change(p =>
            {
                var t = p.T("PlayerEvent");
                if (!p.tables.ContainsKey("PlayerEvent")) throw new InvalidDataException("The profile has no PlayerEvent table.");
                int row = -1;
                for (int r = 0; r < t.rows; r++) if (string.Equals(t.Str("EventId", r), name, StringComparison.Ordinal)) { row = r; break; }
                if (row >= 0) { Set(t, "Status", row, value); return; }
                if (!t.columns.TryGetValue("EventId", out var ids)) t.columns["EventId"] = ids = new List<object>();
                if (!t.columns.TryGetValue("Status", out var status)) t.columns["Status"] = status = new List<object>();
                while (ids.Count < t.rows) ids.Add("");
                while (status.Count < t.rows) status.Add(0.0);
                ids.Add(name); status.Add((double)value); t.rows++;
            });
        }
    }
}

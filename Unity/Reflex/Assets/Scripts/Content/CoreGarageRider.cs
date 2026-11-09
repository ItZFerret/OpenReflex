using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content
{
    public static partial class CoreGarageProfile
    {
        /// The jersey's name and number of the PlayerRider row (the motocard's CHANGE RIDER NAME / NUMBER): Name = up to 8 letters and
        /// digits, Number = up to 3 digits (the original puts "998" for an empty number and "RIDER" for an unusable name).
        public static bool SetRiderText(string column, string value)
        {
            string text = (value ?? "").Trim();
            if (column == "Number") { if (text.Length == 0) text = "998"; if (!Regex.IsMatch(text, "^[0-9]{1,3}$")) { Error = "The number is up to 3 digits."; return false; } }
            else if (column == "Name") { text = text.ToUpperInvariant(); if (text.Length == 0) text = "RIDER"; if (!Regex.IsMatch(text, "^[A-Z0-9]{1,8}$")) { Error = "The name is up to 8 letters and digits."; return false; } }
            else { Error = "Unknown rider field."; return false; }
            return Change(p =>
            {
                var t = p.T("PlayerRider");
                if (t.rows != 1) throw new InvalidDataException("Garage requires one local rider.");
                if (!t.columns.TryGetValue(column, out var cells)) t.columns[column] = cells = new List<object>();
                while (cells.Count < t.rows) cells.Add(null);
                cells[0] = text;
                if (column == "Name" && t.columns.ContainsKey("NameStatus")) Set(t, "NameStatus", 0, 0);
            });
        }
    }
}
